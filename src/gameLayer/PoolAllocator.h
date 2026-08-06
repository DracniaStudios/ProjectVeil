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
	
public:
	PoolAllocator()
	{
		// Pre-allocate contiguous memory big enough for all elements
		// Ensure each block is at least alrge enough to hold a poitner
		
		constexpr size_t objectSize = sizeof(Object) > sizeof(Node*) ? sizeof(Object) : sizeof(Node*);
		poolBuffer = ::operator new[](ObjectCount* objectSize);
	
		// Link all blocks together into an initial free list
		freeList = reinterpret_cast<Node*>(poolBuffer);
		Node* current = freeList;
		for (size_t i = 0; i < ObjectCount - 1; ++i)
		{
			char* nextAddress = reinterpret_cast<char*>(current) + objectSize;
			current->next = reinterpret_cast<Node*>(nextAddress);
			current = current->next;
		}
		current->next = nullptr; // End of List

	}

	~PoolAllocator()
	{
		::operator delete[](poolBuffer);
	}

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
