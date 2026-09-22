#pragma once
#ifndef VIRTUAL_ARENA_H
#define VIRTUAL_ARENA_H

#include <MemoryDebug.h>
#include <VirtualMemory.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>

/**
 * An Arena that grows, by reserving address space up front and committing
 * pages as it fills.
 *
 * Arena has to be sized correctly at construction: too small and it hands back
 * nullptr mid-frame, too large and the whole block is resident from startup.
 * That trade is fine for per-frame scratch, where highWaterMark() turns sizing
 * into a measurement. It is not fine for data whose size is only known once it
 * arrives -- a level's object count, a save file's contents, a room streamed in
 * while another is live.
 *
 * The usual fix is a list of blocks, which costs the contiguity that was the
 * reason to use an arena at all. This does it the other way round: reserve a
 * large address range once (free -- a reservation costs no physical memory) and
 * commit pages into it as the offset advances. The block is contiguous, grows
 * on demand, and -- the property that matters most -- never moves. A pointer
 * handed out when the arena held 64 KiB is still valid after it has grown to
 * 200 MB, which is exactly what a std::vector cannot promise.
 *
 * What it is not: still no per-allocation free, still no destructors. Reaching
 * for this instead of Arena is a statement about size, not about lifetime.
 * Objects that die individually belong in PoolAllocator.h or SlotMap.h either
 * way, and a natural pairing is to put the pool's backing store here --
 * ArenaPool<T>(levelArena, count) gives per-slot recycling inside storage that
 * itself frees in one go at level unload.
 *
 * Sizing the reserve: on 64-bit, be generous. Reserving 4 GB for a level arena
 * that will use 50 MB costs nothing but address space, and the arena then
 * cannot fail for a reason the player would notice. On a 32-bit build the whole
 * user address space is 2-4 GB shared with everything else, so keep reserves in
 * the tens of megabytes there -- the constructor throws if the reservation
 * cannot be met, rather than handing back an arena that fails on first use.
 */
class VirtualArena
{
public:
	struct Marker
	{
		std::size_t offset = 0;
	};

	/**
	 * Reserves `reserveBytes` of address space and commits `initialCommitBytes`
	 * of it immediately.
	 *
	 * Committing nothing up front is the cheapest start and the usual choice;
	 * pre-committing is worth it when the first frame's allocations are already
	 * known and the page faults would land somewhere timing-sensitive.
	 *
	 * Throws std::bad_alloc if the reservation fails. Unlike try_allocate,
	 * which must not take a frame down, this runs at setup time where there is
	 * no sensible way to continue: an arena with no address space behind it can
	 * only return nullptr forever.
	 */
	explicit VirtualArena(std::size_t reserveBytes, std::size_t initialCommitBytes = 0)
	{
		reserved = VeilMemory::AlignUp(reserveBytes, VeilMemory::CommitGranularity);
		if (reserved == 0) { throw std::bad_alloc(); }

		base = static_cast<std::byte*>(VeilMemory::ReserveAddressSpace(reserved));
		if (!base) { throw std::bad_alloc(); }

		if (initialCommitBytes > 0 && !growCommit(initialCommitBytes))
		{
			VeilMemory::ReleaseAddressSpace(base, reserved);
			base = nullptr;
			throw std::bad_alloc();
		}
	}

	~VirtualArena()
	{
		if (base)
		{
			// Same reason as Arena's destructor: the address range goes back to
			// the OS, and leaving ASan's shadow marked would make a later,
			// unrelated mapping at the same address look poisoned.
			VeilMemory::AsanUnpoison(base, committed);
			VeilMemory::ReleaseAddressSpace(base, reserved);
		}
	}

	VirtualArena(const VirtualArena&) = delete;
	VirtualArena& operator=(const VirtualArena&) = delete;

	// Non-throwing allocation, growing the committed range if it needs to.
	// Returns nullptr only once the whole reservation is exhausted or the
	// system refuses the pages. `alignment` must be a power of two, as it must
	// be for std::align and for operator new.
	void* try_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept
	{
		if (alignment < VeilMemory::MinimumAlignment) { alignment = VeilMemory::MinimumAlignment; }

		// Align the absolute address rather than the offset. A reservation's
		// base is only page-aligned on POSIX, so an offset that is a multiple
		// of the requested alignment says nothing about the address it names;
		// rounding the offset alone would hand back a misaligned pointer for
		// any request coarser than the page size.
		const std::uintptr_t origin = reinterpret_cast<std::uintptr_t>(base);
		const std::uintptr_t mask = static_cast<std::uintptr_t>(alignment) - 1;
		if (origin + offset > UINTPTR_MAX - mask) { return nullptr; }

		const std::size_t start = static_cast<std::size_t>(
			(((origin + offset) + mask) & ~mask) - origin);

		// Both checks matter, and the second alone is not enough: start can
		// overshoot the reservation when the alignment is coarser than the
		// granularity it was reserved at, and `reserved - start` would then
		// wrap to a huge value that every size compares smaller than.
		if (start > reserved) { return nullptr; }
		if (bytes > reserved - start) { return nullptr; }      // cannot fit, even fully grown

		const std::size_t end = start + bytes;
		if (end > committed && !growCommit(end)) { return nullptr; }

		offset = end;
		if (offset > highWater) { highWater = offset; }

		void* result = base + start;
		VeilMemory::MarkAllocated(result, bytes);
		return result;
	}

	void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
	{
		if (void* p = try_allocate(bytes, alignment)) { return p; }
		throw std::bad_alloc();
	}

	// Same contract as Arena::create/createArray, including the reason for the
	// static_assert: nothing here ever runs a destructor.
	template <class T, class... Args>
	T* create(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
	{
		static_assert(std::is_trivially_destructible_v<T>,
			"VirtualArena runs no destructors. Use PoolAllocator/SlotMap for types that own anything.");

		void* memory = try_allocate(sizeof(T), alignof(T));
		if (!memory) { return nullptr; }
		return ::new (memory) T(std::forward<Args>(args)...);
	}

	template <class T>
	T* createArray(std::size_t count)
	{
		static_assert(std::is_trivially_destructible_v<T>,
			"VirtualArena runs no destructors. Use PoolAllocator/SlotMap for types that own anything.");

		if (count == 0 || count > SIZE_MAX / sizeof(T)) { return nullptr; }

		void* memory = try_allocate(count * sizeof(T), alignof(T));
		if (!memory) { return nullptr; }

		T* items = static_cast<T*>(memory);
		for (std::size_t i = 0; i < count; ++i) { ::new (items + i) T(); }
		return items;
	}

	/**
	 * Rewinds to empty, keeping every page committed.
	 *
	 * The pages are the expensive part -- each one cost a fault to commit --
	 * and a level arena that reloads the same level wants them back
	 * immediately. Use resetAndDecommit() to return the memory instead, which
	 * is the right call when the next thing to load is much smaller or when
	 * nothing will load for a while.
	 */
	void reset() noexcept
	{
		VeilMemory::MarkReclaimed(base, offset);
		offset = 0;
	}

	// Rewinds to empty and hands the physical pages back to the OS, keeping the
	// reservation so the address range stays ours and every future allocation
	// still lands at the same addresses.
	void resetAndDecommit() noexcept
	{
		reset();
		if (committed > 0)
		{
			VeilMemory::AsanUnpoison(base, committed);
			VeilMemory::DecommitPages(base, committed);
			committed = 0;
		}
	}

	Marker mark() const noexcept { return Marker{ offset }; }

	void rewind(Marker marker) noexcept
	{
		if (marker.offset >= offset) { return; }

		VeilMemory::MarkReclaimed(base + marker.offset, offset - marker.offset);
		offset = marker.offset;
	}

	class Scope
	{
	public:
		explicit Scope(VirtualArena& target) noexcept : arena(target), saved(target.mark()) {}
		~Scope() { arena.rewind(saved); }

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

		Marker marker() const noexcept { return saved; }

	private:
		VirtualArena& arena;
		Marker saved;
	};

	std::size_t used() const noexcept { return offset; }

	// How far the arena can grow. capacity() names the same thing as Arena's,
	// so code templated over either reads the same.
	std::size_t capacity() const noexcept { return reserved; }
	std::size_t remaining() const noexcept { return reserved - offset; }

	// Pages actually backed by physical memory. This, not capacity(), is what
	// the arena costs right now.
	std::size_t committedBytes() const noexcept { return committed; }

	std::size_t highWaterMark() const noexcept { return highWater; }

private:
	// Commits far enough to cover `target` bytes from the base, rounded out to
	// the commit granularity so growth is a syscall every 64 KiB rather than
	// one per page.
	bool growCommit(std::size_t target) noexcept
	{
		if (target <= committed) { return true; }

		std::size_t wanted = VeilMemory::AlignUp(target, VeilMemory::CommitGranularity);
		if (wanted == 0 || wanted > reserved) { wanted = reserved; }
		if (wanted <= committed) { return false; }

		if (!VeilMemory::CommitPages(base + committed, wanted - committed)) { return false; }

		// Freshly committed pages join the arena's unused tail, which is
		// poisoned like the rest of it until try_allocate hands a piece out.
		VeilMemory::AsanPoison(base + committed, wanted - committed);
		committed = wanted;
		return true;
	}

	std::byte* base = nullptr;
	std::size_t reserved = 0;
	std::size_t committed = 0;
	std::size_t offset = 0;
	std::size_t highWater = 0;
};

#endif
