#pragma once
#ifndef SLOT_MAP_H
#define SLOT_MAP_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * Objects in one packed array, addressed by a handle that goes stale safely
 * when the object dies.
 *
 * The problem this solves is not speed, it is the dangling reference. An
 * entity is destroyed while the AI director, a trigger volume and a save
 * record still refer to it. With raw pointers that is a use-after-free. With
 * ids into a map it is worse in one specific way: the id gets reused, and the
 * stale reference silently starts naming a *different* object rather than
 * nothing. Delete "slot 5, Alice", spawn "slot 5, Bob", and every reference to
 * Alice now points at Bob with no error anywhere.
 *
 * A handle is {index, generation}. The generation is bumped every time a slot
 * is freed, so a handle from before the free no longer matches what the slot
 * holds, and get() returns nullptr instead of Bob. That check is two integer
 * comparisons.
 *
 * Storage is dense, which is the second reason to use this. Live objects sit
 * contiguously in one array with no holes, so iterating them is a linear scan
 * -- the same property that took this codebase's O(n^2) broad phase from
 * 35.2 ms to 5.4 ms at N=1000 (see ArenaAllocator.h). A sparse slot table maps
 * a handle's index onto a position in that array, and the position changes as
 * objects are removed, which is exactly why callers must not cache what get()
 * returns.
 *
 *     SlotMap<Enemy> enemies;
 *     auto h = enemies.emplace(spawnPoint);
 *
 *     if (Enemy* e = enemies.get(h)) { e->update(dt); }   // check every time
 *
 *     enemies.erase(h);
 *     enemies.get(h);                                     // nullptr, not Bob
 *
 *     for (Enemy& e : enemies) { e.draw(); }              // contiguous
 *
 * THE ONE RULE: hold handles, not pointers. Every pointer from get(), data()
 * or an iterator is invalidated by the next insert (the array may reallocate)
 * or erase (the last element is swapped into the hole). Handles survive both.
 *
 * Iteration order is unspecified and changes as objects are erased, because
 * erase fills the hole with the last element rather than shifting. Where a
 * stable order matters -- rendering back to front, deterministic replay --
 * sort at the point of use or keep the ordering key in the object.
 */
template <class T>
class SlotMap
{
	// erase() closes the hole by moving the last object into it, which is what
	// keeps the storage dense. Asserting it here turns a type that cannot do
	// that into one clear message instead of a page of vector instantiation
	// errors pointing at <bits/stl_algobase.h>.
	static_assert(std::is_move_assignable_v<T>,
		"SlotMap moves the last object into an erased slot, so T must be move-assignable.");

public:
	/**
	 * A reference to an object in this map.
	 *
	 * Templated on T so a SlotMap<Enemy>::Handle cannot be passed to a
	 * SlotMap<Pickup>, which is a mistake bare integer ids make easy and the
	 * compiler cannot see.
	 *
	 * A default-constructed handle is null and never matches anything: live
	 * slots carry generations from 1 upward, so generation 0 is free to mean
	 * "refers to nothing". Without that reservation a zeroed handle would be a
	 * perfectly valid reference to the first object ever created.
	 */
	struct Handle
	{
		std::uint32_t index = 0;
		std::uint32_t generation = 0;

		constexpr bool valid() const noexcept { return generation != 0; }
		explicit constexpr operator bool() const noexcept { return valid(); }

		friend constexpr bool operator==(Handle, Handle) noexcept = default;

		// Packed form, for storing a reference in a save file or squeezing one
		// into a struct that is counting bytes.
		constexpr std::uint64_t bits() const noexcept
		{
			return (static_cast<std::uint64_t>(generation) << 32) | index;
		}

		static constexpr Handle fromBits(std::uint64_t packed) noexcept
		{
			return Handle{
				static_cast<std::uint32_t>(packed & 0xFFFFFFFFu),
				static_cast<std::uint32_t>(packed >> 32) };
		}
	};

	// Largest number of slots addressable by a 32-bit index, the last value
	// being reserved as the free-list terminator. emplace returns a null
	// handle rather than wrapping once a new slot would exceed this.
	static constexpr std::size_t MaxSlots = 0xFFFFFFFFu;

	template <class... Args>
	Handle emplace(Args&&... args)
	{
		const bool needsNewSlot = (freeHead == NoSlot);

		// The ceiling is the slot table rather than the object count: slot
		// indices are 32-bit and 0xFFFFFFFF is spoken for as the free-list
		// terminator, so one more slot here would be indistinguishable from
		// "no slot". Reusing a free slot is always fine, however full it is.
		if (needsNewSlot && slots.size() >= MaxSlots) { return Handle{}; }

		// Grow every vector's capacity first. Each reserve can throw, and one
		// that does here leaves the map exactly as it was -- no slot popped
		// off the free list, no half-inserted object. After this point the
		// only thing that can throw is T's own constructor, and that happens
		// before anything else is touched.
		items.reserve(items.size() + 1);
		itemSlots.reserve(itemSlots.size() + 1);
		if (needsNewSlot) { slots.reserve(slots.size() + 1); }

		items.emplace_back(std::forward<Args>(args)...);

		std::uint32_t slotIndex;
		if (needsNewSlot)
		{
			slotIndex = static_cast<std::uint32_t>(slots.size());
			slots.push_back(Slot{ NoSlot, 1 });   // generations start live at 1
		}
		else
		{
			slotIndex = freeHead;
			freeHead = slots[slotIndex].dense;    // free slots chain through `dense`
		}

		itemSlots.push_back(slotIndex);
		slots[slotIndex].dense = static_cast<std::uint32_t>(items.size() - 1);

		return Handle{ slotIndex, slots[slotIndex].generation };
	}

	Handle insert(const T& value) { return emplace(value); }
	Handle insert(T&& value) { return emplace(std::move(value)); }

	// Returns nullptr for a null handle, an out-of-range one, or one whose
	// object has since been erased. Call it every time rather than caching the
	// result -- that is the whole contract.
	T* get(Handle handle) noexcept
	{
		if (!alive(handle)) { return nullptr; }
		return &items[slots[handle.index].dense];
	}

	const T* get(Handle handle) const noexcept
	{
		if (!alive(handle)) { return nullptr; }
		return &items[slots[handle.index].dense];
	}

	bool contains(Handle handle) const noexcept { return alive(handle); }

	/**
	 * Destroys the object a handle names. Returns false if the handle was
	 * already stale, which makes a double-erase harmless rather than corrupting
	 * the free list.
	 *
	 * The live object is removed by moving the last one into its place, so the
	 * array stays packed. That moved object keeps its handle -- only its
	 * position changes, and its slot is updated to match.
	 */
	bool erase(Handle handle) noexcept
	{
		if (!alive(handle)) { return false; }

		const std::uint32_t slotIndex = handle.index;
		const std::uint32_t dense = slots[slotIndex].dense;
		const std::uint32_t last = static_cast<std::uint32_t>(items.size() - 1);

		if (dense != last)
		{
			items[dense] = std::move(items[last]);
			itemSlots[dense] = itemSlots[last];
			slots[itemSlots[dense]].dense = dense;   // the moved object's handle still works
		}

		items.pop_back();
		itemSlots.pop_back();

		retire(slotIndex);
		return true;
	}

	// Destroys everything, invalidating every outstanding handle. Slots are
	// recycled, so handles issued afterwards may reuse the same indices -- with
	// higher generations, so the old ones still do not match.
	void clear() noexcept
	{
		for (std::uint32_t slotIndex : itemSlots) { retire(slotIndex); }

		items.clear();
		itemSlots.clear();
	}

	// Drops every slot as well as every object, releasing the memory. Handles
	// issued before this are not merely stale but meaningless, since generation
	// counters restart -- only safe once nothing holds one.
	void reset() noexcept
	{
		items.clear();
		itemSlots.clear();
		slots.clear();
		freeHead = NoSlot;
	}

	void reserve(std::size_t count)
	{
		items.reserve(count);
		itemSlots.reserve(count);
		slots.reserve(count);
	}

	// The packed array of live objects. Contiguous, in unspecified order.
	T* data() noexcept { return items.data(); }
	const T* data() const noexcept { return items.data(); }

	std::size_t size() const noexcept { return items.size(); }
	bool empty() const noexcept { return items.empty(); }

	T* begin() noexcept { return items.data(); }
	T* end() noexcept { return items.data() + items.size(); }
	const T* begin() const noexcept { return items.data(); }
	const T* end() const noexcept { return items.data() + items.size(); }

	// The handle for the object at a position in the packed array, so a loop
	// over the dense storage can still hand out references that outlive it.
	// Returns a null handle if the position is out of range.
	Handle handleAt(std::size_t denseIndex) const noexcept
	{
		if (denseIndex >= itemSlots.size()) { return Handle{}; }

		const std::uint32_t slotIndex = itemSlots[denseIndex];
		return Handle{ slotIndex, slots[slotIndex].generation };
	}

	// How many slots have ever been needed at once. Live objects plus retired
	// slots waiting to be reused.
	std::size_t slotCount() const noexcept { return slots.size(); }

private:
	static constexpr std::uint32_t NoSlot = 0xFFFFFFFFu;

	struct Slot
	{
		// Position in the packed array while live; the next free slot's index
		// while dead. Overlapping the two costs nothing and keeps the slot
		// table at eight bytes an entry.
		std::uint32_t dense = NoSlot;
		std::uint32_t generation = 1;
	};

	/**
	 * The generation match alone decides this, and that is exact rather than
	 * approximate. The invariant: a handle {i, g} exists with
	 * g == slots[i].generation if and only if slot i is live right now. Every
	 * retire bumps the generation past what any outstanding handle holds, and
	 * the only place a handle is ever issued is emplace, immediately after the
	 * slot becomes live. So a free slot cannot match a handle anyone has.
	 *
	 * Testing slots[i].dense would NOT add safety -- a free slot's `dense`
	 * field holds the next free index, not a marker -- and would suggest a
	 * second line of defence that is not there.
	 */
	bool alive(Handle handle) const noexcept
	{
		return handle.generation != 0            // cheap out for the null handle
			&& handle.index < slots.size()
			&& slots[handle.index].generation == handle.generation;
	}

	// Bumps a slot's generation and returns it to the free list.
	void retire(std::uint32_t slotIndex) noexcept
	{
		Slot& slot = slots[slotIndex];

		++slot.generation;

		// Generation 0 is reserved for the null handle, so skip it on wrap.
		// A slot has to be recycled four billion times to get here, but the
		// failure if it were not handled is a live object matching a null
		// handle, which is the one thing this type exists to prevent.
		if (slot.generation == 0) { slot.generation = 1; }

		slot.dense = freeHead;
		freeHead = slotIndex;
	}

	std::vector<T> items;                    // dense: the live objects
	std::vector<std::uint32_t> itemSlots;    // dense -> slot, parallel to items
	std::vector<Slot> slots;                 // sparse: handle index -> position
	std::uint32_t freeHead = NoSlot;
};

#endif
