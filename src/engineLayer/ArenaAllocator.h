#pragma once
#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <MemoryDebug.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>

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
 * individual lifetimes want PoolAllocator.h or SlotMap.h; this is for data with
 * ONE lifetime.
 *
 * It also runs no destructors -- reset() moves an integer. Anything with a
 * non-trivial destructor, and in this codebase anything owning a raylib handle
 * (see GameObject::releaseGeneratedModel), must not live here. create<T>() and
 * createArray<T>() enforce that with a static_assert rather than leaving it to
 * the comment; try_allocate() is the raw form and still trusts the caller.
 *
 * The intended shape is per-frame scratch: build, use, reset at the end of the
 * frame, and never call the general allocator in between. FrameAllocator.h wraps
 * two of these for the common case where this frame reads last frame's results.
 *
 * Capacity is fixed at construction. When the size cannot be known up front --
 * a level whose object count depends on the file being loaded -- VirtualArena.h
 * is the same allocator over a reserved address range that commits pages as it
 * grows, so it can start near zero and expand without ever moving what it
 * already handed out.
 *
 * LIFO use: mark() and rewind() turn this into a stack allocator for nested
 * scopes, which is the shape physics solvers want per step (Box2D's
 * b2StackAllocator, Jolt's TempAllocatorImpl). Arena::Scope does it with RAII.
 */
class Arena
{
public:
	/**
	 * A saved bump offset, taken by mark() and restored by rewind().
	 *
	 * A distinct type rather than a bare size_t on purpose: the arena's other
	 * size-like quantities are byte counts, and rewind(someByteCount) would
	 * compile happily while meaning something entirely different.
	 */
	struct Marker
	{
		std::size_t offset = 0;
	};

	explicit Arena(std::size_t bytes)
		// Rounded up so the block's end lands on an ASan shadow granule.
		// Without it the final partial granule cannot be represented, and the
		// last few bytes of the arena are either permanently unusable or
		// permanently unpoisoned. Costs at most seven bytes.
		: size(VeilMemory::AlignUp(bytes, VeilMemory::MinimumAlignment)), offset(0), highWater(0)
	{
		buffer = std::make_unique<std::byte[]>(size);

		// The whole block starts unaddressable; try_allocate opens up exactly
		// what it hands out. Under ASan this is what makes a pointer into the
		// arena's unused tail -- or into a generation that has been reset --
		// trap on use instead of reading plausible bytes.
		VeilMemory::AsanPoison(buffer.get(), size);
	}

	~Arena()
	{
		// The buffer goes back to the general allocator, which will reuse it
		// for something that has no idea part of it is marked unaddressable.
		// Leaving it poisoned turns the next unrelated allocation into a
		// phantom use-after-free report.
		VeilMemory::AsanUnpoison(buffer.get(), size);
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
	void* try_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept
	{
		// Every chunk starts on an 8-byte boundary so the poisoning above is
		// exact. Raising a 1- or 4-byte request costs a few bytes of padding
		// and buys a shadow map that actually matches the block layout.
		if (alignment < VeilMemory::MinimumAlignment) { alignment = VeilMemory::MinimumAlignment; }

		void* current_ptr = buffer.get() + offset;
		std::size_t space_left = size - offset;

		// Ensure returned memory matches hardware alignment requirements
		if (!std::align(alignment, bytes, current_ptr, space_left)) { return nullptr; }

		// Recalculate offset based on the aligned pointer location
		offset = static_cast<std::byte*>(current_ptr) - buffer.get() + bytes;
		if (offset > highWater) { highWater = offset; }

		VeilMemory::MarkAllocated(current_ptr, bytes);
		return current_ptr;
	}

	// Throwing form, kept for callers that genuinely cannot continue without the
	// memory. Everything on the frame path should prefer try_allocate.
	void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
	{
		if (void* p = try_allocate(bytes, alignment)) { return p; }
		throw std::bad_alloc(); // Out of arena memory
	}

	/**
	 * Constructs one T in the arena, or returns nullptr if it will not fit.
	 *
	 * The static_assert is the arena's central rule made mechanical: reset()
	 * moves an integer and runs nothing, so a T with a destructor that matters
	 * would simply never have it called. Types that own a raylib handle, an
	 * FMOD voice, a std::string or any other resource belong in a pool or a
	 * slot map, both of which do run destructors.
	 */
	template <class T, class... Args>
	T* create(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
	{
		static_assert(std::is_trivially_destructible_v<T>,
			"Arena::reset() runs no destructors. Use PoolAllocator/SlotMap for types that own anything.");

		void* memory = try_allocate(sizeof(T), alignof(T));
		if (!memory) { return nullptr; }
		return ::new (memory) T(std::forward<Args>(args)...);
	}

	// Value-initialises an array of T, or returns nullptr if it will not fit.
	// count == 0 yields nullptr: there is nothing to hand back, and returning a
	// pointer callers must not dereference is worse than a null they already
	// have to check.
	template <class T>
	T* createArray(std::size_t count)
	{
		static_assert(std::is_trivially_destructible_v<T>,
			"Arena::reset() runs no destructors. Use PoolAllocator/SlotMap for types that own anything.");

		// count * sizeof(T) wraps for large counts, and a wrapped size sails
		// through the space check and hands back a block far too small.
		if (count == 0 || count > SIZE_MAX / sizeof(T)) { return nullptr; }

		void* memory = try_allocate(count * sizeof(T), alignof(T));
		if (!memory) { return nullptr; }

		T* items = static_cast<T*>(memory);
		for (std::size_t i = 0; i < count; ++i) { ::new (items + i) T(); }
		return items;
	}

	void reset() noexcept
	{
		// Instant teardown of all allocated memory. Nothing is freed and no
		// destructor runs; the generation that was live simply stops being
		// reachable through any new allocation.
		VeilMemory::MarkReclaimed(buffer.get(), offset);
		offset = 0;
	}

	// Current bump offset, to be handed back to rewind() later.
	Marker mark() const noexcept { return Marker{ offset }; }

	/**
	 * Releases everything allocated since the marker was taken.
	 *
	 * Strictly LIFO: rewinding past a block while something still points into
	 * it is the same mistake as using memory after reset(). A marker from a
	 * later generation (one taken before a reset() that has since happened) is
	 * ignored rather than trusted -- restoring it would move the offset
	 * forward over memory the arena considers free and hand the same bytes out
	 * twice.
	 */
	void rewind(Marker marker) noexcept
	{
		if (marker.offset >= offset) { return; }

		VeilMemory::MarkReclaimed(buffer.get() + marker.offset, offset - marker.offset);
		offset = marker.offset;
	}

	/**
	 * RAII form of mark()/rewind(): everything allocated inside the scope is
	 * released when it exits, however it exits.
	 *
	 *     void BuildNavQuery(Arena& scratch)
	 *     {
	 *         Arena::Scope scope(scratch);
	 *         Node* open = scratch.createArray<Node>(count);
	 *         ...                      // early return or throw is fine
	 *     }                            // scratch rewound to entry
	 */
	class Scope
	{
	public:
		explicit Scope(Arena& target) noexcept : arena(target), saved(target.mark()) {}
		~Scope() { arena.rewind(saved); }

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

		Marker marker() const noexcept { return saved; }

	private:
		Arena& arena;
		Marker saved;
	};

	std::size_t used() const noexcept { return offset; }
	std::size_t capacity() const noexcept { return size; }
	std::size_t remaining() const noexcept { return size - offset; }

	// Peak usage since construction; survives reset(), which is what makes it
	// usable for sizing the arena from a reading rather than a guess.
	std::size_t highWaterMark() const noexcept { return highWater; }

private:
	std::unique_ptr<std::byte[]> buffer;
	std::size_t size;
	std::size_t offset;
	std::size_t highWater;
};

#endif
