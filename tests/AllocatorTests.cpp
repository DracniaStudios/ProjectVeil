/**
 * Standalone tests for the memory utilities.
 *
 * The cheapest tier in the suite: ArenaAllocator.h and PoolAllocator.h are
 * header-only with no dependency on raylib, FMOD or anything in the engine, so
 * this links against nothing at all. If this file ever needs a library to build,
 * an allocator has grown a dependency it should not have.
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

/** Arena **/

static void TestArenaHandsOutDistinctAlignedBlocks()
{
	std::printf("the arena hands out distinct, correctly aligned blocks\n");
	Arena arena(1024);

	void* a = arena.try_allocate(16, 16);
	void* b = arena.try_allocate(16, 16);

	Check(a != nullptr && b != nullptr, "both allocations succeed");
	Check(a != b, "two allocations never share an address");
	Check(reinterpret_cast<std::uintptr_t>(a) % 16 == 0, "the first block honours its alignment");
	Check(reinterpret_cast<std::uintptr_t>(b) % 16 == 0, "the second block honours its alignment");

	// A block must not overlap the one after it, whatever padding alignment added.
	auto* first = static_cast<std::byte*>(a);
	auto* second = static_cast<std::byte*>(b);
	Check(second >= first + 16, "the second block starts past the end of the first");
}

static void TestArenaOverAlignedRequest()
{
	std::printf("a request stricter than max_align_t is still honoured\n");
	Arena arena(512);
	// Deliberately waste a byte first, so the next 64-byte request cannot be
	// satisfied without the aligner moving the pointer forward.
	arena.try_allocate(1, 1);
	void* p = arena.try_allocate(64, 64);
	Check(p != nullptr, "a 64-byte-aligned block is available");
	Check(p != nullptr && reinterpret_cast<std::uintptr_t>(p) % 64 == 0, "and it really is 64-byte aligned");
}

static void TestArenaExhaustionReturnsNullRatherThanThrowing()
{
	std::printf("exhaustion returns nullptr from try_allocate, and throws from allocate\n");
	Arena arena(64);

	Check(arena.try_allocate(64, 1) != nullptr, "a request that exactly fills the arena succeeds");
	Check(arena.try_allocate(1, 1) == nullptr, "the next byte returns nullptr rather than throwing");

	// This is the property the frame path depends on: a full arena must not take
	// the game down mid-frame.
	bool threw = false;
	try { arena.allocate(1, 1); } catch (const std::bad_alloc&) { threw = true; }
	Check(threw, "the throwing form still throws, for callers that want it");
}

static void TestArenaResetReusesTheSameMemory()
{
	std::printf("reset() rewinds to the start and hands the same memory back\n");
	Arena arena(256);

	void* first = arena.try_allocate(128, alignof(std::max_align_t));
	Check(arena.used() == 128, "used() tracks the bump offset");
	Check(arena.remaining() == 128, "remaining() is the rest of the block");

	arena.reset();
	Check(arena.used() == 0, "reset() returns the offset to zero");
	Check(arena.remaining() == arena.capacity(), "and the whole arena is available again");

	void* second = arena.try_allocate(128, alignof(std::max_align_t));
	Check(first == second, "the next allocation reuses the same address");
}

static void TestArenaHighWaterSurvivesReset()
{
	std::printf("the high-water mark survives reset, so the arena can be sized from a reading\n");
	Arena arena(1024);

	arena.try_allocate(300, 1);
	arena.reset();
	arena.try_allocate(100, 1);

	Check(arena.used() == 100, "used() reflects only the current generation");
	Check(arena.highWaterMark() >= 300, "the high-water mark remembers the busiest generation");

	arena.reset();
	Check(arena.highWaterMark() >= 300, "and it is not cleared by a later reset");
}

/** Pool **/

namespace
{
	int g_liveProbes = 0;

	// Non-trivial on purpose: the pool must run the destructor, which is exactly
	// what the arena does NOT do and why fat objects belong here instead.
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
	Check(a != nullptr && a->name == "alpha" && a->value == 1, "the first object holds its arguments");
	Check(b != nullptr && b->name == "beta" && b->value == 2, "the second object holds its arguments");
	Check(a != b, "two live objects never share a slot");

	pool.destroy(a);
	Check(g_liveProbes == 1, "destroy() ran the destructor");

	pool.destroy(b);
	Check(g_liveProbes == 0, "and again for the second");

	pool.destroy(nullptr); // documented no-op
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
	try { pool.construct("c", 3); } catch (const std::bad_alloc&) { threw = true; }
	Check(threw, "the third construct() on a two-slot pool throws");

	pool.destroy(a);
	pool.destroy(b);
	Check(g_liveProbes == 0, "everything still tears down cleanly afterwards");
}

static void TestPoolStrideHandlesAwkwardSizes()
{
	std::printf("slots do not overlap for a type smaller than the free-list pointer\n");
	// The regression the header documents: striding by sizeof(Object) rather than
	// sizeof(Node) wrote each `next` pointer into the middle of the previous slot.
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

int main()
{
	std::printf("Allocator tests\n\n");

	TestArenaHandsOutDistinctAlignedBlocks();
	TestArenaOverAlignedRequest();
	TestArenaExhaustionReturnsNullRatherThanThrowing();
	TestArenaResetReusesTheSameMemory();
	TestArenaHighWaterSurvivesReset();

	TestPoolConstructsAndDestroys();
	TestPoolRecyclesFreedSlots();
	TestPoolExhaustionThrows();
	TestPoolStrideHandlesAwkwardSizes();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
