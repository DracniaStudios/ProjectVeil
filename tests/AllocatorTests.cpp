/**
 * Standalone tests for the memory utilities.
 *
 * The cheapest tier in the suite: ArenaAllocator.h and PoolAllocator.h are
 * header-only with no dependency on raylib, FMOD or anything in the engine, so
 * this links against nothing at all. If this file ever needs a library to build,
 * an allocator has grown a dependency it should not have.
 *
 * Ported from the suite on claude/arena-pool-allocators-gamemap-svn5i4, which
 * tested an Arena with try_allocate() and a high-water mark. Neither is on this
 * branch, so these cases test the Arena that is: allocate() throws when full.
 *
 * Build and run: tests/run_tests.sh
 */

#include <ArenaAllocator.h>
#include <PoolAllocator.h>

#include <cstdint>
#include <cstdio>
#include <new>
#include <string>
#include <vector>

using MemoryAllocator::PoolAllocator;

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

static bool AlignedTo(const void* pointer, std::size_t alignment)
{
	return reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

// allocate() throws when the arena is full. Caught here so a broken arena fails
// a check instead of ending the run with an uncaught bad_alloc.
static void* TryAllocate(Arena& arena, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
{
	try { return arena.allocate(bytes, alignment); }
	catch (const std::bad_alloc&) { return nullptr; }
}

// ─── Arena ─────────────────────────────────────────────────────────────────

static void TestArenaHandsOutDistinctAlignedBlocks()
{
	std::printf("the arena hands out distinct, correctly aligned blocks\n");
	Arena arena(1024);

	void* a = TryAllocate(arena, 16, 16);
	void* b = TryAllocate(arena, 16, 16);

	Check(a != b, "two allocations never share an address");
	Check(AlignedTo(a, 16), "the first block honours its alignment");
	Check(AlignedTo(b, 16), "the second block honours its alignment");

	// A block must not overlap the one after it, whatever padding alignment added
	Check(static_cast<std::byte*>(b) >= static_cast<std::byte*>(a) + 16, "the second block starts past the end of the first");
}

static void TestArenaDefaultAlignmentIsMaxAlign()
{
	std::printf("a block asked for without an alignment is aligned for any type\n");
	Arena arena(256);

	TryAllocate(arena, 1, 1); // leave the offset odd
	void* p = TryAllocate(arena, 8);

	Check(AlignedTo(p, alignof(std::max_align_t)), "the default alignment is max_align_t");
}

static void TestArenaOverAlignedRequest()
{
	std::printf("a request stricter than max_align_t is still honoured\n");
	Arena arena(512);

	// Waste a byte first, so the 64-byte request cannot be met without the
	// aligner moving the pointer forward
	TryAllocate(arena, 1, 1);
	void* p = TryAllocate(arena, 64, 64);

	Check(AlignedTo(p, 64), "a 64-byte-aligned block really is 64-byte aligned");
}

static void TestArenaExhaustionThrows()
{
	std::printf("a full arena throws bad_alloc instead of overrunning its block\n");
	Arena arena(64);

	Check(TryAllocate(arena, 64, 1) != nullptr, "a request that exactly fills the arena succeeds");
	Check(TryAllocate(arena, 1, 1) == nullptr, "the next byte throws");
	Check(TryAllocate(arena, 1, 1) == nullptr, "and so does every byte after it");
}

static void TestArenaTooLargeRequestThrows()
{
	std::printf("a request larger than the whole arena throws on an empty arena\n");
	Arena arena(32);

	Check(TryAllocate(arena, 33, 1) == nullptr, "33 bytes do not fit in 32");
}

static void TestArenaFailedRequestTakesNothing()
{
	std::printf("a request that does not fit leaves the arena as it was\n");
	Arena arena(64);

	TryAllocate(arena, 40, 1);
	Check(TryAllocate(arena, 32, 1) == nullptr, "32 bytes do not fit in the 24 that are left");
	Check(TryAllocate(arena, 24, 1) != nullptr, "the 24 bytes that were left are still there");
}

static void TestArenaAlignmentPaddingCountsAgainstCapacity()
{
	std::printf("padding for alignment is taken from the arena's capacity\n");
	Arena arena(64);

	// One byte is left, at offset 63. The block itself is at least 16-aligned,
	// so that byte never is
	TryAllocate(arena, 63, 1);
	Check(TryAllocate(arena, 1, 16) == nullptr, "a 16-aligned byte does not fit in the last, unaligned byte");
	Check(TryAllocate(arena, 1, 1) != nullptr, "the same byte asked for unaligned still fits");
}

static void TestArenaResetReusesTheSameMemory()
{
	std::printf("reset() rewinds to the start and hands the same memory back\n");
	Arena arena(256);

	void* first = TryAllocate(arena, 128);
	TryAllocate(arena, 128);
	Check(TryAllocate(arena, 1, 1) == nullptr, "the arena is full before reset");

	arena.reset();
	void* second = TryAllocate(arena, 128);

	Check(first == second, "the first allocation after reset reuses the first address");
	Check(TryAllocate(arena, 128, 1) != nullptr, "the whole block is available again");
}

static void TestArenaStoresWhatIsWritten()
{
	std::printf("values written into separate blocks keep their own contents\n");
	Arena arena(1024);

	std::vector<int*> blocks;
	for (int i = 0; i < 32; ++i)
	{
		int* value = static_cast<int*>(TryAllocate(arena, sizeof(int), alignof(int)));
		if (value == nullptr) { Check(false, "32 ints fit in 1024 bytes"); return; }
		*value = i * 7;
		blocks.push_back(value);
	}

	bool intact = true;
	for (int i = 0; i < 32; ++i)
	{
		if (*blocks[i] != i * 7) { intact = false; }
	}
	Check(intact, "every block still holds its own value once the rest are written");
}

// ─── Pool ──────────────────────────────────────────────────────────────────

namespace
{
	int g_liveProbes = 0;

	// Non-trivial on purpose: the pool must run the destructor, which the arena
	// never does and is why objects with their own lifetimes belong in a pool
	struct Probe
	{
		std::string name;
		int value = 0;

		Probe(std::string n, int v) : name(std::move(n)), value(v) { ++g_liveProbes; }
		~Probe() { --g_liveProbes; }
	};
}

static void TestPoolConstructsAndDestroys()
{
	std::printf("the pool constructs in place and runs the destructor on destroy\n");
	g_liveProbes = 0;
	PoolAllocator<Probe, 4> pool;

	Probe* a = pool.construct("alpha", 1);
	Probe* b = pool.construct("beta", 2);

	Check(g_liveProbes == 2, "two constructed objects are live");
	Check(a->name == "alpha" && a->value == 1, "the first object holds its arguments");
	Check(b->name == "beta" && b->value == 2, "the second object holds its arguments");
	Check(a != b, "two live objects never share a slot");

	pool.destroy(a);
	Check(g_liveProbes == 1, "destroy() ran the destructor");

	pool.destroy(b);
	Check(g_liveProbes == 0, "and again for the second");

	pool.destroy(nullptr);
	Check(g_liveProbes == 0, "destroying nullptr is a no-op");
}

static void TestPoolRecyclesFreedSlots()
{
	std::printf("a freed slot is handed straight back out\n");
	g_liveProbes = 0;
	PoolAllocator<Probe, 2> pool;

	Probe* a = pool.construct("first", 1);
	pool.destroy(a);
	Probe* b = pool.construct("second", 2);

	Check(a == b, "the most recently freed slot is reused first");
	Check(b->name == "second", "and the slot holds the new object, not the old one");
	pool.destroy(b);
}

static void TestPoolExhaustionThrows()
{
	std::printf("a full pool throws rather than overrunning its buffer\n");
	g_liveProbes = 0;
	PoolAllocator<Probe, 2> pool;

	Probe* a = pool.construct("a", 1);
	Probe* b = pool.construct("b", 2);

	bool threw = false;
	try { pool.construct("c", 3); }
	catch (const std::bad_alloc&) { threw = true; }
	Check(threw, "the third construct() on a two-slot pool throws");
	Check(g_liveProbes == 2, "the refused object was never constructed");

	pool.destroy(a);
	pool.destroy(b);
	Check(g_liveProbes == 0, "everything still tears down cleanly afterwards");
}

static void TestPoolRegainsCapacityAfterFreeing()
{
	std::printf("emptying a full pool lets it be filled again\n");
	g_liveProbes = 0;
	PoolAllocator<Probe, 3> pool;

	for (int round = 0; round < 3; ++round)
	{
		std::vector<Probe*> live;
		bool threw = false;
		try
		{
			for (int i = 0; i < 3; ++i) { live.push_back(pool.construct("p", i)); }
		}
		catch (const std::bad_alloc&) { threw = true; }

		Check(!threw, "round " + std::to_string(round) + " fits every slot");
		for (Probe* p : live) { pool.destroy(p); }
	}
	Check(g_liveProbes == 0, "no object outlives its round");
}

static void TestPoolStrideHandlesAwkwardSizes()
{
	std::printf("slots do not overlap for a type smaller than the free-list pointer\n");

	// The regression the header documents: striding by sizeof(Object) rather
	// than sizeof(Node) wrote each `next` pointer into the middle of the
	// previous slot
	struct Small { std::uint8_t a; };
	PoolAllocator<Small, 8> pool;

	std::vector<Small*> live;
	for (int i = 0; i < 8; ++i)
	{
		Small* p = pool.construct();
		p->a = static_cast<std::uint8_t>(i + 1);
		live.push_back(p);
	}

	bool allDistinct = true;
	for (std::size_t i = 0; i < live.size(); ++i)
	{
		if (live[i]->a != static_cast<std::uint8_t>(i + 1)) { allDistinct = false; }
	}
	Check(allDistinct, "every slot still holds its own value after the pool is filled");

	for (Small* p : live) { pool.destroy(p); }
}

static void TestPoolHonoursTheObjectsAlignment()
{
	std::printf("every slot is aligned for an over-aligned type\n");

	struct alignas(32) Wide { float lanes[8]; };
	PoolAllocator<Wide, 5> pool;

	std::vector<Wide*> live;
	for (int i = 0; i < 5; ++i) { live.push_back(pool.construct()); }

	bool aligned = true;
	for (Wide* p : live)
	{
		if (!AlignedTo(p, 32)) { aligned = false; }
	}
	Check(aligned, "all five slots are 32-byte aligned");

	for (Wide* p : live) { pool.destroy(p); }
}

int main()
{
	std::printf("Allocator tests\n\n");

	TestArenaHandsOutDistinctAlignedBlocks();
	TestArenaDefaultAlignmentIsMaxAlign();
	TestArenaOverAlignedRequest();
	TestArenaExhaustionThrows();
	TestArenaTooLargeRequestThrows();
	TestArenaFailedRequestTakesNothing();
	TestArenaAlignmentPaddingCountsAgainstCapacity();
	TestArenaResetReusesTheSameMemory();
	TestArenaStoresWhatIsWritten();

	TestPoolConstructsAndDestroys();
	TestPoolRecyclesFreedSlots();
	TestPoolExhaustionThrows();
	TestPoolRegainsCapacityAfterFreeing();
	TestPoolStrideHandlesAwkwardSizes();
	TestPoolHonoursTheObjectsAlignment();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
