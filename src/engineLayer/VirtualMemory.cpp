#include "VirtualMemory.h"

#if defined(_WIN32)

	// <raylib.h> arrives ahead of this through the precompiled header, and it
	// defines a Rectangle struct plus CloseWindow/ShowCursor/LoadImage/DrawText
	// that collide head-on with the Win32 declarations of the same names. NOGDI
	// and NOUSER drop the GDI and USER32 sections that declare them, and
	// WIN32_LEAN_AND_MEAN drops the rest of the shell/networking surface this
	// file has no use for. VirtualAlloc/VirtualFree/GetSystemInfo live in the
	// base API, which none of these switches touch.
	//
	// Do not remove these, and do not include <windows.h> from a header.
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOGDI
		#define NOGDI
	#endif
	#ifndef NOUSER
		#define NOUSER
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>

#elif defined(__unix__) || defined(__APPLE__)

	#include <sys/mman.h>
	#include <unistd.h>

	// Linux honours MAP_NORESERVE; it is absent or inert on some other unices,
	// where a large PROT_NONE reserve is not charged against commit limits
	// anyway. Zero is the documented "no extra flags" value.
	#ifndef MAP_NORESERVE
		#define MAP_NORESERVE 0
	#endif

#else
	#error "VirtualMemory has no implementation for this platform."
#endif

namespace VeilMemory
{

#if defined(_WIN32)

	std::size_t PageSize() noexcept
	{
		SYSTEM_INFO info{};
		GetSystemInfo(&info);
		return static_cast<std::size_t>(info.dwPageSize);
	}

	void* ReserveAddressSpace(std::size_t bytes) noexcept
	{
		if (bytes == 0) { return nullptr; }

		// PAGE_NOACCESS, so a stray pointer into the reserved-but-uncommitted
		// tail faults here rather than silently working once the arena grows
		// far enough to cover it.
		return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS);
	}

	bool CommitPages(void* base, std::size_t bytes) noexcept
	{
		if (!base || bytes == 0) { return bytes == 0; }
		return VirtualAlloc(base, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
	}

	bool DecommitPages(void* base, std::size_t bytes) noexcept
	{
		if (!base || bytes == 0) { return bytes == 0; }
		return VirtualFree(base, bytes, MEM_DECOMMIT) != 0;
	}

	void ReleaseAddressSpace(void* base, std::size_t bytes) noexcept
	{
		// MEM_RELEASE frees the entire original reservation and requires a
		// size of zero; passing the real size is an error here, unlike munmap.
		(void)bytes;
		if (base) { VirtualFree(base, 0, MEM_RELEASE); }
	}

#else

	std::size_t PageSize() noexcept
	{
		const long queried = ::sysconf(_SC_PAGESIZE);
		return queried > 0 ? static_cast<std::size_t>(queried) : 4096;
	}

	void* ReserveAddressSpace(std::size_t bytes) noexcept
	{
		if (bytes == 0) { return nullptr; }

		// PROT_NONE is the reservation: address space with no access, and no
		// pages behind it. MAP_NORESERVE keeps a large reserve from being
		// charged against the overcommit limit on Linux, which is what makes
		// reserving far more than will ever be committed safe to do.
		void* base = ::mmap(nullptr, bytes, PROT_NONE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

		return base == MAP_FAILED ? nullptr : base;
	}

	bool CommitPages(void* base, std::size_t bytes) noexcept
	{
		if (!base || bytes == 0) { return bytes == 0; }

		// Committing is just granting access: the kernel supplies zeroed pages
		// on first touch.
		return ::mprotect(base, bytes, PROT_READ | PROT_WRITE) == 0;
	}

	bool DecommitPages(void* base, std::size_t bytes) noexcept
	{
		if (!base || bytes == 0) { return bytes == 0; }

		// MADV_DONTNEED is what actually hands the physical pages back; the
		// mprotect alone would leave them resident. On Linux a later commit
		// then reads as zeroes, matching Win32; macOS treats the hint more
		// loosely and may return the same contents. Neither matters here --
		// the arena has never promised zeroed memory, and stamps 0xCD over
		// what it hands out in debug builds regardless.
		::madvise(base, bytes, MADV_DONTNEED);
		return ::mprotect(base, bytes, PROT_NONE) == 0;
	}

	void ReleaseAddressSpace(void* base, std::size_t bytes) noexcept
	{
		if (base && bytes) { ::munmap(base, bytes); }
	}

#endif

}
