#pragma once
#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include <MemoryDebug.h>

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

/// 
/// The Pool Allocator assumes that each object is of the same size.
/// Duplicated the objects and stores them into a memory array.
/// Where they can be removed and added individually.
/// 
/// The pool automatically reuses space where the last particle (earliest created) is removed.
///
/// Unlike Arena, this DOES run destructors -- destroy() calls ~Object before
/// returning the slot -- which is why types that own a raylib handle, an FMOD
/// voice or any other resource belong here rather than in an arena.
///
/// What it does not do is notice when an object outlives the pointer you kept
/// to it. A freed slot is handed straight back out, so a stale pointer starts
/// referring to a different object of the same type with no way to tell. Where
/// other systems hold references to objects that can die at arbitrary times --
/// entities, in particular -- SlotMap.h is the same idea with a generation
/// counter that makes the stale reference detectable.
///

/// @tparam Object 
/// @tparam ObjectCount
template <typename Object, size_t ObjectCount>
class PoolAllocator
{
	static_assert(ObjectCount > 0, "A pool needs at least one slot");

public:
	PoolAllocator()
	{
		// Pre-allocate contiguous memory big enough for all elements.
		//
		// Both the size and the stride are sizeof(Node), NOT
		// max(sizeof(Object), sizeof(Node*)). Node is a union of the object
		// storage and the free-list pointer, so the compiler pads it out to the
		// stricter of the two alignments — for a 12-byte, 4-aligned Object on a
		// 64-bit target that is 16 bytes against the 12 the old expression
		// produced. Walking the buffer in 12-byte steps then wrote each `next`
		// pointer into the middle of the preceding slot and ran the last one
		// four bytes past the end of the allocation.
		poolBuffer = ::operator new[](ObjectCount * sizeof(Node), std::align_val_t{ alignof(Node) });

		// Link all blocks together into an initial free list
		Node* nodes = static_cast<Node*>(poolBuffer);
		freeList = nodes;
		for (size_t i = 0; i + 1 < ObjectCount; ++i)
		{
			nodes[i].next = &nodes[i + 1];
		}
		nodes[ObjectCount - 1].next = nullptr; // End of List

		// Every slot starts free, so every slot starts poisoned.
		for (size_t i = 0; i < ObjectCount; ++i) { PoisonFreeSlot(&nodes[i]); }
	}

	~PoolAllocator()
	{
		// Anything still live is NOT destroyed here: the pool cannot enumerate
		// its live slots, only its free ones. Call destroy() for each object,
		// or use a type whose destructor does not matter.
		//
		// The unpoisoning is not optional. This buffer goes back to the general
		// allocator, which will reuse it for something with no idea parts of it
		// are marked unaddressable.
		VeilMemory::AsanUnpoison(poolBuffer, ObjectCount * sizeof(Node));

		// Must mirror the aligned new above — pairing a plain operator delete[]
		// with an aligned operator new[] is undefined.
		::operator delete[](poolBuffer, std::align_val_t{ alignof(Node) });
	}

	// The pool owns a raw buffer and hands out interior pointers, so a copy
	// would double-free it and leave the original's live objects pointing into
	// freed memory. Arena already forbids this for the same reason.
	PoolAllocator(const PoolAllocator&) = delete;
	PoolAllocator& operator=(const PoolAllocator&) = delete;
	PoolAllocator(PoolAllocator&&) = delete;
	PoolAllocator& operator=(PoolAllocator&&) = delete;

	// Allocate memory and construct the object
	template <typename... Args>
	Object* construct(Args&&... args)
	{
		if (!freeList)
		{
			throw std::bad_alloc(); // Pool is entirely full
		}

		return constructInFreeSlot(std::forward<Args>(args)...);
	}

	/**
	 * Non-throwing form: returns nullptr when the pool is full.
	 *
	 * The counterpart to Arena::try_allocate, and for the same reason. A pool
	 * sized for the common case -- particles, projectiles, audio voices -- will
	 * occasionally run out on a busy frame, and dropping the spawn is almost
	 * always the right response. Taking the game down is not.
	 */
	template <typename... Args>
	Object* try_construct(Args&&... args)
	{
		if (!freeList) { return nullptr; }
		return constructInFreeSlot(std::forward<Args>(args)...);
	}
	
	// Call destructor and return memory back to the pool
	void destroy(Object* ptr) noexcept
	{
		if (!ptr) return;

		ptr->~Object(); // Explicitly run destructor

		Node* node = reinterpret_cast<Node*>(ptr);

		// Stamp BEFORE threading the node back onto the free list: `next`
		// lives in these same bytes, and writing it first would just be
		// overwritten by the fill. Poisoning comes last, for the same reason --
		// PoisonFreeSlot deliberately leaves `next` readable.
		VeilMemory::FillFreed(node, sizeof(Node));

		node->next = freeList;
		freeList = node;
		--live;

		PoisonFreeSlot(node);
	}

	static constexpr size_t capacity() noexcept { return ObjectCount; }

	size_t liveCount() const noexcept { return live; }
	size_t remaining() const noexcept { return ObjectCount - live; }
	bool full() const noexcept { return freeList == nullptr; }
	bool empty() const noexcept { return live == 0; }

	// Bytes the pool occupies, which is ObjectCount * sizeof(Node) and not
	// ObjectCount * sizeof(Object) -- see the stride note in the constructor.
	static constexpr size_t bytesUsed() noexcept { return ObjectCount * sizeof(Node); }

private:
	union Node
	{
		Node* next;
		alignas(Object) char storage[sizeof(Object)];
	};

	template <typename... Args>
	Object* constructInFreeSlot(Args&&... args)
	{
		// Pop a node from the free list
		// Read `next` before MarkAllocated stamps 0xCD over it -- the free
		// pointer and the object share these bytes. PoisonFreeSlot leaves the
		// first granule readable precisely so this load needs no unpoisoning.
		Node* node = freeList;
		freeList = node->next;

		VeilMemory::MarkAllocated(node, sizeof(Node));
		++live;

		// Placement new to construct the object in that slot
		return ::new (static_cast<void*>(node)) Object(std::forward<Args>(args)...);
	}

	// A free slot is unreadable except for the `next` pointer threaded through
	// its first bytes, which the pool itself must keep reading.
	//
	// Worth doing only when the slots line up with ASan's 8-byte shadow
	// granularity. When they do not -- a small or weakly aligned Object on a
	// 32-bit target -- a poisoned range would spill into the neighbouring slot
	// or fail to cover this one, so the pool skips ASan rather than reporting
	// on memory it did not mean to. The 0xCD/0xDD fills in MarkAllocated and
	// FillFreed work regardless and are unaffected.
	static constexpr bool AsanUsable =
		(alignof(Node) >= VeilMemory::MinimumAlignment) &&
		(sizeof(Node) % VeilMemory::MinimumAlignment == 0);

	static void PoisonFreeSlot(Node* node) noexcept
	{
		if constexpr (AsanUsable)
		{
			if constexpr (sizeof(Node) > VeilMemory::MinimumAlignment)
			{
				VeilMemory::AsanPoison(
					reinterpret_cast<char*>(node) + VeilMemory::MinimumAlignment,
					sizeof(Node) - VeilMemory::MinimumAlignment);
			}
		}
		else
		{
			(void)node;
		}
	}

	void* poolBuffer = nullptr;
	Node* freeList = nullptr;
	size_t live = 0;

};


/**
 * The same pool over memory somebody else owns -- normally an arena.
 *
 * PoolAllocator fixes its slot count at compile time and owns its buffer, which
 * is right for a pool whose size is a design decision (256 audio voices, 1024
 * particles). It cannot express a pool whose size is only known at runtime, and
 * it cannot put its storage anywhere in particular.
 *
 * This is the pairing the two allocators are actually for. An arena gives one
 * contiguous block with one lifetime; a free list threaded through that block
 * gives the individual objects inside it their own lifetimes. Storage and
 * object lifetime stop being the same question:
 *
 *     Arena levelArena(8 * 1024 * 1024);
 *     ArenaPool<Projectile> projectiles(levelArena, maxProjectiles);
 *
 *     Projectile* p = projectiles.try_construct(spawn);   // individual
 *     projectiles.destroy(p);                             // individual
 *     ...
 *     levelArena.reset();                                 // all of it, at once
 *
 * The pool does not own the memory and must not outlive it. After the arena is
 * reset or rewound past the pool's block, the pool is dangling -- treat it as
 * dead and build a new one, exactly as you would a pointer into the arena.
 */
template <typename Object>
class ArenaPool
{
public:
	// An empty pool that hands out nothing. Lets a pool be a member that is
	// built later, once its owning arena and size are known.
	ArenaPool() = default;

	// Adopts `slotCount` slots' worth of memory at `memory`, which must be at
	// least bytesFor(slotCount) bytes and aligned to slotAlignment().
	ArenaPool(void* memory, size_t slotCount) noexcept
	{
		adopt(memory, slotCount);
	}

	/**
	 * Takes the pool's storage out of any arena in this folder.
	 *
	 * Templated on the arena type rather than taking Arena& so the same pool
	 * works over a VirtualArena, and so this header keeps depending on nothing
	 * but MemoryDebug.h. A pool whose arena cannot supply the block is left
	 * valid() == false and hands out nothing, which is the same non-throwing
	 * contract as try_allocate itself.
	 */
	template <typename ArenaLike>
	ArenaPool(ArenaLike& arena, size_t slotCount) noexcept
	{
		const size_t bytes = bytesFor(slotCount);
		if (bytes == 0) { return; }

		adopt(arena.try_allocate(bytes, slotAlignment()), slotCount);
	}

	ArenaPool(const ArenaPool&) = delete;
	ArenaPool& operator=(const ArenaPool&) = delete;

	// Movable, unlike PoolAllocator: this one owns no buffer, so moving it is
	// just moving the head of a free list. The moved-from pool is left empty.
	ArenaPool(ArenaPool&& other) noexcept
		: slots(other.slots), slotCount(other.slotCount), freeList(other.freeList), live(other.live)
	{
		other.slots = nullptr;
		other.slotCount = 0;
		other.freeList = nullptr;
		other.live = 0;
	}

	ArenaPool& operator=(ArenaPool&& other) noexcept
	{
		if (this != &other)
		{
			slots = other.slots;
			slotCount = other.slotCount;
			freeList = other.freeList;
			live = other.live;

			other.slots = nullptr;
			other.slotCount = 0;
			other.freeList = nullptr;
			other.live = 0;
		}
		return *this;
	}

	// No destructor: the memory belongs to the arena, and live objects are the
	// caller's to destroy() for the same reason as PoolAllocator.

	bool valid() const noexcept { return slots != nullptr; }

	template <typename... Args>
	Object* try_construct(Args&&... args)
	{
		if (!freeList) { return nullptr; }

		Node* node = freeList;
		freeList = node->next;

		VeilMemory::MarkAllocated(node, sizeof(Node));
		++live;

		return ::new (static_cast<void*>(node)) Object(std::forward<Args>(args)...);
	}

	template <typename... Args>
	Object* construct(Args&&... args)
	{
		if (Object* p = try_construct(std::forward<Args>(args)...)) { return p; }
		throw std::bad_alloc();
	}

	void destroy(Object* ptr) noexcept
	{
		if (!ptr) return;

		ptr->~Object();

		Node* node = reinterpret_cast<Node*>(ptr);
		VeilMemory::FillFreed(node, sizeof(Node));

		node->next = freeList;
		freeList = node;
		--live;

		PoisonFreeSlot(node);
	}

	size_t capacity() const noexcept { return slotCount; }
	size_t liveCount() const noexcept { return live; }
	size_t remaining() const noexcept { return slotCount - live; }
	bool full() const noexcept { return freeList == nullptr; }
	bool empty() const noexcept { return live == 0; }

	// Bytes an arena must supply for `slotCount` slots. Returns 0 for a count
	// of zero or one that would overflow, which the constructors treat as
	// "build nothing" rather than allocating a wrapped-around size.
	static constexpr size_t bytesFor(size_t count) noexcept
	{
		if (count == 0 || count > SIZE_MAX / sizeof(Node)) { return 0; }
		return count * sizeof(Node);
	}

	static constexpr size_t slotSize() noexcept { return sizeof(Node); }
	static constexpr size_t slotAlignment() noexcept { return alignof(Node); }

private:
	union Node
	{
		Node* next;
		alignas(Object) char storage[sizeof(Object)];
	};

	static constexpr bool AsanUsable =
		(alignof(Node) >= VeilMemory::MinimumAlignment) &&
		(sizeof(Node) % VeilMemory::MinimumAlignment == 0);

	static void PoisonFreeSlot(Node* node) noexcept
	{
		if constexpr (AsanUsable)
		{
			if constexpr (sizeof(Node) > VeilMemory::MinimumAlignment)
			{
				VeilMemory::AsanPoison(
					reinterpret_cast<char*>(node) + VeilMemory::MinimumAlignment,
					sizeof(Node) - VeilMemory::MinimumAlignment);
			}
		}
		else
		{
			(void)node;
		}
	}

	void adopt(void* memory, size_t count) noexcept
	{
		if (!memory || count == 0 || bytesFor(count) == 0) { return; }

		slots = static_cast<Node*>(memory);
		slotCount = count;

		// The arena handed these bytes out, so they are unpoisoned and stamped
		// 0xCD. Thread the free list through them, then poison each slot back
		// down to just its `next` pointer.
		for (size_t i = 0; i + 1 < count; ++i) { slots[i].next = &slots[i + 1]; }
		slots[count - 1].next = nullptr;
		freeList = slots;

		for (size_t i = 0; i < count; ++i) { PoisonFreeSlot(&slots[i]); }
	}

	Node* slots = nullptr;
	size_t slotCount = 0;
	Node* freeList = nullptr;
	size_t live = 0;
};


#endif
