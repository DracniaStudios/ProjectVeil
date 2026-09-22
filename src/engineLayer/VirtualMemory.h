#pragma once
#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include <cstddef>

/**
 * The four OS calls VirtualArena needs, and nothing else.
 *
 * Split out of VirtualArena.h rather than inlined into it because the Win32
 * side needs <windows.h>, and <windows.h> and <raylib.h> cannot share a
 * translation unit without care -- both declare Rectangle, CloseWindow,
 * ShowCursor, LoadImage and DrawText. Every game TU gets <raylib.h> force
 * included through the precompiled header (see CMakeLists.txt), so a header
 * that pulls in <windows.h> would break the MSVC build for anything that
 * included it. Keeping the platform code in one .cpp confines the problem to a
 * file that includes neither the PCH's raylib nor anything else.
 *
 * Reserving is not committing. A reservation claims address space and costs no
 * physical memory -- a 4 GB reserve on a 64-bit build is free until touched.
 * Committing backs part of that range with pages that count against RAM. That
 * split is what lets an arena start at nothing and grow without ever moving:
 * the address range is nailed down up front, so every pointer already handed
 * out stays valid no matter how far it grows.
 */
namespace VeilMemory
{
	// Granularity every commit is rounded to. Chosen once rather than queried
	// per call: 64 KiB is Windows' allocation granularity and a whole multiple
	// of every page size these targets use (4 KiB x86/x64, 16 KiB Apple
	// silicon, 64 KiB large-page arm64), so a range rounded to it is always
	// page-aligned. Larger than a page on purpose -- committing in page-sized
	// steps turns a growing arena into a syscall per 4 KiB.
	inline constexpr std::size_t CommitGranularity = 64 * 1024;

	// The OS page size, for reporting. Commits round to CommitGranularity.
	std::size_t PageSize() noexcept;

	// Claims `bytes` of address space with no access and no physical backing.
	// Returns nullptr if the range is unavailable -- which is a real outcome on
	// a 32-bit build, where the whole user address space is 2-4 GB.
	void* ReserveAddressSpace(std::size_t bytes) noexcept;

	// Backs [base, base + bytes) with readable/writable pages. The range must
	// lie inside a previous reservation. Committing an already-committed range
	// is harmless. Returns false if the system cannot supply the pages.
	bool CommitPages(void* base, std::size_t bytes) noexcept;

	// Returns [base, base + bytes) to the OS while keeping the reservation, so
	// the address range stays ours and can be committed again later.
	bool DecommitPages(void* base, std::size_t bytes) noexcept;

	// Releases the whole reservation made by ReserveAddressSpace. `bytes` must
	// be the size originally reserved.
	void ReleaseAddressSpace(void* base, std::size_t bytes) noexcept;
}

#endif
