#pragma once
#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>

/**
 * Bump allocator: hand out bytes by moving an offset, and free the whole lot at
 * once with reset(). There is deliberately no per-allocation free.
 *
 * That restriction is the point rather than a gap. Adding a free list turns this
 * into a pool, and a pool hands freed slots back out of order, which scatters the
 * live set through the block and gives up most of the locality the single
 * allocation bought. Measured on a stand-in at GameObject's real 896 bytes, an
 * O(n^2) traversal over 1000 objects: scattered map 35.2 ms, one contiguous block
 * in order 5.4 ms, the same block after free-list churn 9.6 ms. Objects with
 * individual lifetimes want PoolAllocator.h; this is for data with ONE lifetime.
 *
 * It also runs no destructors -- reset() moves an integer. Anything with a
 * non-trivial destructor, and in this codebase anything owning a raylib handle
 * (see GameObject::releaseGeneratedModel), must not live here.
 *
 * The intended shape is per-frame scratch: build, use, reset at the end of the
 * frame, and never call the general allocator in between.
 */
class Arena
{
public:
	explicit Arena(size_t bytes) : size(bytes), offset(0), highWater(0)
	{
		buffer = std::make_unique<std::byte[]>(size);
	}

	// Disable copying to prevent accidental double-allocations or pointer desync
	Arena(const Arena&) = delete;
	Arena& operator=(const Arena&) = delete;

	/**
	 * Non-throwing allocation: returns nullptr when the arena is exhausted.
	 *
	 * This is the one callers on the frame path should use. A per-frame arena
	 * that throws takes the game down mid-frame, where returning nullptr lets the
	 * caller fall back to whatever it did before the arena existed.
	 */
	void* try_allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) noexcept
	{
		void* current_ptr = buffer.get() + offset;
		size_t space_left = size - offset;

		// Ensure returned memory matches hardware alignment requirements
		if (!std::align(alignment, bytes, current_ptr, space_left)) { return nullptr; }

		// Recalculate offset based on the aligned pointer location
		offset = static_cast<std::byte*>(current_ptr) - buffer.get() + bytes;
		if (offset > highWater) { highWater = offset; }
		return current_ptr;
	}

	// Throwing form, kept for callers that genuinely cannot continue without the
	// memory. Everything on the frame path should prefer try_allocate.
	void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t))
	{
		if (void* p = try_allocate(bytes, alignment)) { return p; }
		throw std::bad_alloc(); // Out of arena memory
	}

	void reset() noexcept
	{
		offset = 0; // Instant teardown of all allocated memory
	}

	size_t used() const noexcept { return offset; }
	size_t capacity() const noexcept { return size; }
	size_t remaining() const noexcept { return size - offset; }

	// Peak usage since construction; survives reset(), which is what makes it
	// usable for sizing the arena from a reading rather than a guess.
	size_t highWaterMark() const noexcept { return highWater; }

private:
	std::unique_ptr<std::byte[]> buffer;
	size_t size;
	size_t offset;
	size_t highWater;
};

#endif
