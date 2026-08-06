#pragma once
#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <GameObject.h>

class Arena
{
public:
	explicit Arena(size_t bytes) : size(bytes), offset(0)
	{
		buffer = std::make_unique<std::vector<GameObject>[]>(size);
	}

	// Disable copying to prevent accidental double-allocations or pointer desync
	Arena(const Arena&) = delete;
	Arena& operator=(const Arena&) = delete;

	void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t))
	{
		void* current_ptr = buffer.get() + offset;
		size_t space_left = size - offset;

		// Ensure returned memory matches hardware alignment requirements
		if (std::align(alignment, bytes, current_ptr, space_left))
		{
			// Recalculate offset based on the aligned pointer location
			offset = static_cast<std::vector<GameObject>*>(current_ptr) - buffer.get() + bytes;
			return current_ptr;
		}

		throw std::bad_alloc(); // Out of arena memory
	}

	void reset() noexcept
	{
		offset = 0; // Instant teardown of all allocated memory
	}

private:
	std::unique_ptr<std::vector<GameObject>[]> buffer;
	size_t size;
	size_t offset;
};

#endif
