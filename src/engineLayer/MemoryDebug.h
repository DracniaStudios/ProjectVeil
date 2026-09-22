#pragma once
#ifndef MEMORY_DEBUG_H
#define MEMORY_DEBUG_H

#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * Poison patterns and AddressSanitizer plumbing shared by every allocator here.
 *
 * A custom allocator hides exactly the bugs the platform allocator catches for
 * you. Hand a slot back to a pool and the bytes stay readable, still holding a
 * plausible-looking object, so a use-after-free reads stale-but-sane data and
 * the symptom surfaces somewhere else entirely. Reset an arena and every
 * pointer into it silently aliases whatever the next frame builds there.
 *
 * Two independent mechanisms address that, and they are deliberately separate:
 *
 *   Fill patterns (VEIL_MEMORY_DEBUG) write 0xCD over freshly handed-out bytes
 *   and 0xDD over reclaimed ones. Cheap, always available, and they turn "sane
 *   stale data" into a value obvious in a debugger -- a float reads as about
 *   -4.3e8, a pointer as 0xDDDDDDDDDDDDDDDD, which faults on dereference
 *   instead of wandering off into live memory.
 *
 *   ASan poisoning (VEIL_MEMORY_ASAN) marks reclaimed bytes unaddressable in
 *   the sanitizer's shadow map, so the *read itself* traps with a stack trace
 *   rather than merely producing a recognisable value. Only active under
 *   -fsanitize=address; a no-op otherwise.
 *
 * On granularity: ASan's shadow is 8-byte granular, so a poisoned region that
 * does not start on an 8-byte boundary cannot be represented exactly. ASan
 * resolves that conservatively on its own -- poisoning may cover less than
 * asked, unpoisoning may cover more -- which never produces a false report,
 * only a missed one. Allocators here still keep their chunks 8-byte aligned
 * (Arena raises every request to MinimumAlignment) so the poisoning actually
 * bites; a pool whose slot stride defeats that skips ASan rather than
 * pretending, see PoolAllocator.h.
 */

// Fill patterns cost a memset per allocation and per reset, so they follow the
// build's own debug switch. Define VEIL_MEMORY_DEBUG yourself to override --
// 1 to keep them in a release build that is chasing a corruption bug, 0 to
// drop them from a debug build that is profiling.
#ifndef VEIL_MEMORY_DEBUG
	#ifdef NDEBUG
		#define VEIL_MEMORY_DEBUG 0
	#else
		#define VEIL_MEMORY_DEBUG 1
	#endif
#endif

#ifndef VEIL_MEMORY_ASAN
	#if defined(__has_feature)
		#if __has_feature(address_sanitizer)
			#define VEIL_MEMORY_ASAN 1
		#endif
	#endif
	#if !defined(VEIL_MEMORY_ASAN) && defined(__SANITIZE_ADDRESS__)
		#define VEIL_MEMORY_ASAN 1
	#endif
	#ifndef VEIL_MEMORY_ASAN
		#define VEIL_MEMORY_ASAN 0
	#endif
#endif

#if VEIL_MEMORY_ASAN
	#include <sanitizer/asan_interface.h>
#endif

namespace VeilMemory
{
	// Written over bytes just handed to a caller. Anything still reading this
	// pattern was never initialised by whoever asked for it.
	inline constexpr std::uint8_t FreshPattern = 0xCD;

	// Written over bytes taken back. Reading this is a use-after-free.
	inline constexpr std::uint8_t FreedPattern = 0xDD;

	// ASan's shadow granularity. Chunks start here so poisoning is exact.
	inline constexpr std::size_t MinimumAlignment = 8;

	inline void FillFresh(void* memory, std::size_t bytes) noexcept
	{
	#if VEIL_MEMORY_DEBUG
		if (memory && bytes) { std::memset(memory, FreshPattern, bytes); }
	#else
		(void)memory; (void)bytes;
	#endif
	}

	inline void FillFreed(void* memory, std::size_t bytes) noexcept
	{
	#if VEIL_MEMORY_DEBUG
		if (memory && bytes) { std::memset(memory, FreedPattern, bytes); }
	#else
		(void)memory; (void)bytes;
	#endif
	}

	// Mark a region unaddressable. ASan may poison less than asked when the
	// region is not 8-aligned; it never poisons past it.
	inline void AsanPoison(const void* memory, std::size_t bytes) noexcept
	{
	#if VEIL_MEMORY_ASAN
		if (memory && bytes) { ASAN_POISON_MEMORY_REGION(memory, bytes); }
	#else
		(void)memory; (void)bytes;
	#endif
	}

	// Mark a region addressable again. ASan may unpoison a little more than
	// asked for the same granularity reason, which can only lose a report.
	inline void AsanUnpoison(const void* memory, std::size_t bytes) noexcept
	{
	#if VEIL_MEMORY_ASAN
		if (memory && bytes) { ASAN_UNPOISON_MEMORY_REGION(memory, bytes); }
	#else
		(void)memory; (void)bytes;
	#endif
	}

	// Bytes are leaving the allocator: make them accessible, then stamp them so
	// an unwritten read is recognisable. Unpoison first -- the fill is itself a
	// write, and would trap on still-poisoned memory.
	inline void MarkAllocated(void* memory, std::size_t bytes) noexcept
	{
		AsanUnpoison(memory, bytes);
		FillFresh(memory, bytes);
	}

	/**
	 * Bytes are coming back to the allocator: stamp them, then take away
	 * access to the whole span.
	 *
	 * The unpoison first is not redundant. A span being reclaimed in bulk --
	 * an arena reset or rewind -- covers more than the chunks that were handed
	 * out: the alignment padding between them was never unpoisoned, and is
	 * still marked unaddressable. The fill is a write, so without opening the
	 * span first it trips over that padding and reports a use-after-poison
	 * against the allocator itself.
	 */
	inline void MarkReclaimed(void* memory, std::size_t bytes) noexcept
	{
	#if VEIL_MEMORY_DEBUG
		AsanUnpoison(memory, bytes);
		FillFreed(memory, bytes);
	#endif
		AsanPoison(memory, bytes);
	}

	// Round up to a multiple of `alignment`, which must be a power of two.
	// Returns 0 on overflow, which every caller treats as "will not fit".
	inline constexpr std::size_t AlignUp(std::size_t value, std::size_t alignment) noexcept
	{
		const std::size_t mask = alignment - 1;
		if (value > SIZE_MAX - mask) { return 0; }
		return (value + mask) & ~mask;
	}
}

#endif
