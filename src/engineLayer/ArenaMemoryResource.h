#pragma once
#ifndef ARENA_MEMORY_RESOURCE_H
#define ARENA_MEMORY_RESOURCE_H

#include <cstddef>
#include <memory_resource>
#include <new>

/**
 * Exposes any arena in this folder as a std::pmr::memory_resource, so ordinary
 * standard containers can build into it.
 *
 * The allocators here are the right answer for data laid out deliberately, but
 * plenty of per-frame work is just a std::vector that gets filled, read once
 * and thrown away -- a visible-object list, a sorted draw order, a string
 * assembled for a debug overlay. Rewriting those by hand is not worth it; what
 * they cost is the heap traffic, and that is exactly what this removes:
 *
 *     ArenaResource scratch(frames.current());
 *     std::pmr::vector<GameObject*> visible(&scratch);
 *     visible.reserve(256);                 // comes out of the frame arena
 *     ...                                   // grows there too
 *                                           // no free, no destructor needed
 *
 * std::pmr::monotonic_buffer_resource does the same job over its own buffer.
 * This exists because pointing it at *these* arenas keeps one accounting of
 * frame memory -- one highWaterMark() to size from, one reset, one poisoning
 * scheme -- instead of a second pool sitting beside the first.
 *
 * Two things to keep in mind, both inherited from the arena rather than added
 * here. Deallocation is a no-op, so a container that grows repeatedly leaves
 * every superseded buffer behind; reserve() up front when the size is known.
 * And the arena runs no destructors, so the element type must be one that does
 * not need one -- a pmr::vector<std::pmr::string> is fine because the strings'
 * own storage also comes from here, but a vector of raylib-handle owners is
 * not. The container itself must still go out of scope before the arena resets.
 */
template <class ArenaLike>
class ArenaResource final : public std::pmr::memory_resource
{
public:
	explicit ArenaResource(ArenaLike& target) noexcept : arena(&target) {}

	ArenaLike& target() const noexcept { return *arena; }

private:
	void* do_allocate(std::size_t bytes, std::size_t alignment) override
	{
		// pmr's contract is to throw on failure, not to return null: the
		// container has no way to be told the allocation did not happen. This
		// is the one place in these allocators where exhaustion throws by
		// design rather than as the opt-in path.
		void* memory = arena->try_allocate(bytes, alignment);
		if (!memory) { throw std::bad_alloc(); }
		return memory;
	}

	void do_deallocate(void*, std::size_t, std::size_t) override
	{
		// Arenas free in one go. The memory comes back at reset(), not here.
	}

	bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
	{
		// Two resources are interchangeable only if they are the same object
		// over the same arena -- memory from one arena cannot be returned to
		// another, and pmr uses this to decide whether a container can simply
		// adopt another's buffer.
		const auto* rhs = dynamic_cast<const ArenaResource*>(&other);
		return rhs != nullptr && rhs->arena == arena;
	}

	ArenaLike* arena;
};

// Lets `ArenaResource scratch(someArena);` deduce the arena type, so callers
// never have to name Arena or VirtualArena twice.
template <class ArenaLike>
ArenaResource(ArenaLike&) -> ArenaResource<ArenaLike>;

#endif
