#pragma once
#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include <cstddef>
#include <new>
#include <utility>
#include <vector>

/// 
/// The Pool Allocator assumes that each object is of the same size.
/// Duplicated the objects and stores them into a memory array.
/// Where they can be removed and added individually.
/// 
/// The pool automatically reuses space where the last particle (earliest created) is removed.
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

	}

	~PoolAllocator()
	{
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

		// Pop a node from the free list
		Node* node = freeList;
		freeList = freeList->next;

		// Placement new to construct the object in that slot
		return ::new (static_cast<void*>(node)) Object(std::forward<Args>(args)...);
	}
	
	// Call destructor and return memory back to the pool
	void destroy(Object* ptr) noexcept
	{
		if (!ptr) return;

		ptr->~Object(); // Explicitly run destructor

		Node* node = reinterpret_cast<Node*>(ptr);
		node->next = freeList;
		freeList = node;

	}

private:
	union Node
	{
		Node* next;
		alignas(Object) char storage[sizeof(Object)];
	};

	void* poolBuffer = nullptr;
	Node* freeList = nullptr;

};


#endif
