/**
 * Standalone tests for the memory utilities.
 *
 * The cheapest tier in the suite: every allocator here is header-only with no
 * dependency on raylib, FMOD or anything else in the engine, so this links
 * against no library at all. The one .cpp it compiles alongside,
 * VirtualMemory.cpp, is the reserve/commit shim over mmap and VirtualAlloc --
 * OS headers only, and split out of the header precisely so <windows.h> never
 * has to meet <raylib.h>. If this file ever needs a LIBRARY to build, an
 * allocator has grown a dependency it should not have.
 *
 * Worth running under the sanitizer as well as plain, since half of what the
 * allocators do for correctness is only observable there:
 *
 *   g++ -std=c++23 -fsanitize=address -g -I src/engineLayer \
 *       tests/AllocatorTests.cpp src/engineLayer/VirtualMemory.cpp -o asan_tests
 *
 * Build and run: tests/run_tests.sh
 */

#include <ArenaAllocator.h>
#include <ArenaMemoryResource.h>
#include <FrameAllocator.h>
#include <MemoryTracker.h>
#include <PoolAllocator.h>
#include <SlotMap.h>
#include <VirtualArena.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <new>
#include <string>
#include <utility>
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

/** Arena: markers, scopes and typed construction **/

static void TestArenaRewindReleasesBackToMarker()
{
	std::printf("rewind() releases everything allocated since the marker\n");
	Arena arena(1024);

	arena.try_allocate(64, 8);
	const Arena::Marker marker = arena.mark();
	const std::size_t before = arena.used();

	arena.try_allocate(128, 8);
	Check(arena.used() > before, "allocating past the marker moves the offset");

	arena.rewind(marker);
	Check(arena.used() == before, "rewind() returns the offset to the marker");

	// The point of LIFO: the reclaimed bytes are handed straight back out.
	void* reused = arena.try_allocate(128, 8);
	Check(reused != nullptr, "the rewound space is available again");
	Check(arena.used() == before + 128, "and it is the same space, not more");
}

static void TestArenaRewindSpansAlignmentPadding()
{
	std::printf("rewind() covers padding between chunks, not just the chunks\n");
	// A bulk reclaim covers more than what try_allocate handed out: the gap an
	// over-aligned request skipped was never given to anyone. Stamping the span
	// without accounting for that reads as a use-after-poison under ASan, which
	// is the build this test is really for -- it passes trivially otherwise.
	Arena arena(1024);

	const Arena::Marker marker = arena.mark();
	arena.try_allocate(1, 1);      // leaves the offset on an odd byte
	arena.try_allocate(64, 64);    // forces a padding gap in front of it

	arena.rewind(marker);
	Check(arena.used() == 0, "the whole span, padding included, is reclaimed");

	arena.try_allocate(8, 8);
	arena.reset();
	Check(arena.used() == 0, "and reset() over the same shape is fine too");
}

static void TestArenaRewindIgnoresStaleMarkers()
{
	std::printf("a marker from a spent generation cannot move the offset forward\n");
	Arena arena(256);

	arena.try_allocate(128, 8);
	const Arena::Marker stale = arena.mark();   // offset 128

	arena.reset();                               // generation over
	arena.try_allocate(8, 8);                    // offset 8

	arena.rewind(stale);
	// Honouring it would move the offset to 128 and hand out bytes 8..128 a
	// second time, to a caller that already holds them.
	Check(arena.used() == 8, "a marker past the current offset is ignored");
}

static void TestArenaScopeRewindsOnExit()
{
	std::printf("Arena::Scope rewinds however the scope is left\n");
	Arena arena(1024);
	arena.try_allocate(32, 8);
	const std::size_t entry = arena.used();

	{
		Arena::Scope scope(arena);
		arena.try_allocate(256, 8);
		Check(arena.used() > entry, "allocations inside the scope land normally");
	}
	Check(arena.used() == entry, "leaving the scope rewinds to where it started");

	// Nesting is the case markers exist for: an inner scope must not disturb
	// the outer one's mark.
	{
		Arena::Scope outer(arena);
		arena.try_allocate(64, 8);
		const std::size_t afterOuter = arena.used();
		{
			Arena::Scope inner(arena);
			arena.try_allocate(64, 8);
		}
		Check(arena.used() == afterOuter, "the inner scope rewinds only its own");
	}
	Check(arena.used() == entry, "and the outer scope still rewinds all of it");
}

static void TestArenaTypedConstruction()
{
	std::printf("create<T>/createArray<T> construct in place and report failure\n");
	Arena arena(256);

	struct Point { int x; int y; };

	Point* one = arena.create<Point>(3, 4);
	Check(one != nullptr && one->x == 3 && one->y == 4, "create<T> forwards its arguments");
	Check(reinterpret_cast<std::uintptr_t>(one) % alignof(Point) == 0, "and honours the type's alignment");

	Point* many = arena.createArray<Point>(4);
	Check(many != nullptr, "createArray<T> hands back a block");
	Check(many != nullptr && many[0].x == 0 && many[3].y == 0, "whose elements are value-initialised");

	Check(arena.createArray<Point>(0) == nullptr, "a zero-length array is null rather than a pointer to nothing");

	// count * sizeof(T) wraps for counts this large. A wrapped size would pass
	// the space check and hand back a block far too small for the loop that
	// then writes into it.
	Check(arena.createArray<Point>(SIZE_MAX / 2) == nullptr, "an overflowing count is refused, not wrapped");

	Arena tiny(16);
	Check(tiny.createArray<Point>(1000) == nullptr, "and so is a count that simply does not fit");
}

/** VirtualArena **/

static void TestVirtualArenaCommitsOnDemand()
{
	std::printf("the virtual arena reserves up front and commits as it fills\n");
	VirtualArena arena(4 * 1024 * 1024);

	Check(arena.capacity() >= 4 * 1024 * 1024, "the whole reservation is the capacity");
	Check(arena.committedBytes() == 0, "but nothing is committed before anything is asked for");

	void* first = arena.try_allocate(16, 8);
	Check(first != nullptr, "the first allocation succeeds");
	Check(arena.committedBytes() >= 16, "and commits pages to back it");
	Check(arena.committedBytes() < arena.capacity(), "without committing the whole reservation");
}

static void TestVirtualArenaKeepsPointersValidAcrossGrowth()
{
	std::printf("growing never moves what was already handed out\n");
	// This is the property that separates it from a vector, and the reason the
	// reservation is taken up front: the address range cannot move, so growth
	// can only ever extend it.
	VirtualArena arena(8 * 1024 * 1024);

	auto* first = arena.createArray<std::uint32_t>(4);
	Check(first != nullptr, "the first block is handed out");
	for (int i = 0; i < 4; ++i) { first[i] = static_cast<std::uint32_t>(0xA5A50000u + i); }

	const std::size_t committedBefore = arena.committedBytes();

	// Enough to force several rounds of commits past the initial granule.
	for (int i = 0; i < 64; ++i)
	{
		Check(arena.try_allocate(16 * 1024, 8) != nullptr, "growth keeps succeeding");
	}
	Check(arena.committedBytes() > committedBefore, "the committed range really did grow");

	bool intact = true;
	for (int i = 0; i < 4; ++i)
	{
		if (first[i] != static_cast<std::uint32_t>(0xA5A50000u + i)) { intact = false; }
	}
	Check(intact, "the pointer from before the growth still reads its own data");
}

static void TestVirtualArenaExhaustsAtTheReservation()
{
	std::printf("allocation stops at the reservation rather than growing past it\n");
	VirtualArena arena(64 * 1024);
	const std::size_t capacity = arena.capacity();

	Check(arena.try_allocate(capacity, 8) != nullptr, "a request filling the reservation succeeds");
	Check(arena.try_allocate(1, 1) == nullptr, "the next byte returns nullptr");

	bool threw = false;
	try { arena.allocate(1, 1); } catch (const std::bad_alloc&) { threw = true; }
	Check(threw, "and the throwing form still throws");
}

static void TestVirtualArenaOverAlignedRequest()
{
	std::printf("an alignment coarser than the reservation granularity is honoured\n");
	// The reservation's base is only page-aligned, so rounding the offset is
	// not enough -- the absolute address is what has to come out aligned.
	VirtualArena arena(4 * 1024 * 1024);

	arena.try_allocate(1, 1);
	void* p = arena.try_allocate(64, 4096);
	Check(p != nullptr, "the over-aligned block is available");
	Check(p != nullptr && reinterpret_cast<std::uintptr_t>(p) % 4096 == 0, "and really is aligned");
}

static void TestVirtualArenaResetKeepsPagesDecommitReturnsThem()
{
	std::printf("reset() keeps the committed pages; resetAndDecommit() hands them back\n");
	VirtualArena arena(4 * 1024 * 1024);
	arena.try_allocate(256 * 1024, 8);

	const std::size_t committed = arena.committedBytes();
	Check(committed >= 256 * 1024, "pages were committed to serve the allocation");

	arena.reset();
	Check(arena.used() == 0, "reset() empties the arena");
	Check(arena.committedBytes() == committed, "but keeps the pages, which cost faults to get");
	Check(arena.highWaterMark() >= 256 * 1024, "and the high-water mark survives it");

	arena.resetAndDecommit();
	Check(arena.committedBytes() == 0, "resetAndDecommit() returns them to the OS");

	void* again = arena.try_allocate(64, 8);
	Check(again != nullptr, "and the arena still works afterwards");
}

static void TestVirtualArenaMarkers()
{
	std::printf("the virtual arena rewinds to a marker like the fixed one\n");
	VirtualArena arena(1024 * 1024);

	arena.try_allocate(128, 8);
	const VirtualArena::Marker marker = arena.mark();
	{
		VirtualArena::Scope scope(arena);
		arena.try_allocate(4096, 8);
	}
	Check(arena.used() == marker.offset, "the scope rewound to its entry point");
}

/** FrameAllocator **/

static void TestFrameAllocatorKeepsLastFramesData()
{
	std::printf("the frame allocator keeps last frame's data intact through this one\n");
	FrameAllocator frames(4096);

	// Frame N
	auto* written = frames.createArray<std::uint32_t>(4);
	Check(written != nullptr, "this frame allocates from the current arena");
	for (int i = 0; i < 4; ++i) { written[i] = static_cast<std::uint32_t>(100 + i); }
	const std::size_t usedInFirst = frames.current().used();

	// Frame N+1
	frames.nextFrame();
	Check(frames.current().used() == 0, "the new frame starts empty");
	Check(frames.previous().used() == usedInFirst, "and last frame's arena is untouched");

	bool intact = true;
	for (int i = 0; i < 4; ++i)
	{
		if (written[i] != static_cast<std::uint32_t>(100 + i)) { intact = false; }
	}
	Check(intact, "last frame's data is still readable");

	auto* thisFrame = frames.createArray<std::uint32_t>(4);
	Check(thisFrame != nullptr && thisFrame != written, "this frame's allocations land in the other arena");
}

static void TestFrameAllocatorRecyclesTwoBuffers()
{
	std::printf("the pair alternates, so memory is reused rather than accumulated\n");
	FrameAllocator frames(4096);

	void* a = frames.try_allocate(64, 8);
	frames.nextFrame();
	void* b = frames.try_allocate(64, 8);
	frames.nextFrame();
	void* c = frames.try_allocate(64, 8);

	Check(a != b, "consecutive frames use different arenas");
	Check(a == c, "and the frame after that is back in the first one");
	Check(frames.frameIndex() == 2, "frameIndex() counts the flips");
	Check(frames.capacityPerFrame() >= 4096, "each arena carries the full per-frame size");
	Check(frames.highWaterMark() >= 64, "the high-water mark covers both arenas");
}

static void TestArenaResourceBacksStandardContainers()
{
	std::printf("std::pmr containers allocate out of an arena through ArenaResource\n");
	Arena arena(8192);
	ArenaResource resource(arena);

	const std::size_t before = arena.used();
	{
		std::pmr::vector<int> values(&resource);
		values.reserve(64);
		for (int i = 0; i < 64; ++i) { values.push_back(i); }

		Check(arena.used() > before, "the vector's buffer came out of the arena");
		Check(values.size() == 64 && values[63] == 63, "and the vector works normally");
	}

	// The container is gone; the arena is not. Deallocation is a no-op, which
	// is the whole bargain -- memory comes back at reset(), all at once.
	Check(arena.used() > before, "destroying the container frees nothing on its own");
	arena.reset();
	Check(arena.used() == 0, "the arena reclaims it in one go");

	Arena tiny(64);
	ArenaResource tinyResource(tiny);
	bool threw = false;
	try
	{
		std::pmr::vector<int> tooBig(&tinyResource);
		tooBig.reserve(4096);
	}
	catch (const std::bad_alloc&) { threw = true; }
	Check(threw, "exhaustion throws here, because pmr has no way to report a null");
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

/** Pool: the additions **/

static void TestPoolReportsOccupancy()
{
	std::printf("the pool reports how full it is\n");
	g_liveProbes = 0;
	PoolAllocator<Probe, 3> pool;

	Check(pool.capacity() == 3, "capacity() is the compile-time slot count");
	Check(pool.empty() && !pool.full(), "a fresh pool is empty and not full");

	Probe* a = pool.construct("a", 1);
	Probe* b = pool.construct("b", 2);
	Check(pool.liveCount() == 2 && pool.remaining() == 1, "live and remaining track construction");

	Probe* c = pool.construct("c", 3);
	Check(pool.full(), "the pool reports full once the last slot goes");

	pool.destroy(a);
	Check(!pool.full() && pool.liveCount() == 2, "destroying one makes room again");

	pool.destroy(b);
	pool.destroy(c);
	Check(pool.empty() && g_liveProbes == 0, "and everything tears down");
}

static void TestPoolTryConstructReturnsNullWhenFull()
{
	std::printf("try_construct returns nullptr on a full pool rather than throwing\n");
	g_liveProbes = 0;
	PoolAllocator<Probe, 1> pool;

	Probe* a = pool.try_construct("only", 1);
	Check(a != nullptr, "the first slot is handed out");
	Check(pool.try_construct("second", 2) == nullptr, "the second request returns nullptr");

	pool.destroy(a);
	Check(pool.try_construct("again", 3) != nullptr, "and the slot is reusable afterwards");
	Check(g_liveProbes == 1, "with exactly one object live");
}

static void TestArenaPoolLivesInsideAnArena()
{
	std::printf("an arena-backed pool recycles slots inside arena-owned memory\n");
	g_liveProbes = 0;
	Arena arena(4096);

	const std::size_t beforePool = arena.used();
	ArenaPool<Probe> pool(arena, 4);

	Check(pool.valid(), "the pool took its storage from the arena");
	Check(arena.used() >= beforePool + ArenaPool<Probe>::bytesFor(4), "which shows up in the arena's usage");
	Check(pool.capacity() == 4, "capacity is the runtime count it was given");

	Probe* a = pool.try_construct("alpha", 1);
	Probe* b = pool.try_construct("beta", 2);
	Check(a != nullptr && b != nullptr && a != b, "objects are constructed in distinct slots");
	Check(g_liveProbes == 2, "and are really alive");

	pool.destroy(a);
	Check(g_liveProbes == 1, "destroy() runs the destructor, unlike the arena");

	Probe* reused = pool.try_construct("gamma", 3);
	Check(reused == a, "the freed slot is handed straight back out");

	// Long enough to actually allocate, so a missed destructor would show up
	// as a leak rather than hiding in the small-string buffer.
	Probe* d = pool.try_construct("a name past the small-string buffer", 4);
	Probe* e = pool.try_construct("another name past the small-string buffer", 5);
	Check(d != nullptr && e != nullptr, "the remaining slots are handed out");
	Check(pool.full(), "the pool fills at its slot count");
	Check(pool.try_construct("f", 6) == nullptr, "and refuses the next one");

	pool.destroy(reused);
	pool.destroy(b);
	Check(g_liveProbes == 2, "the two never destroyed are still live");

	// Those two are the documented hazard: letting the arena reset here would
	// reclaim their storage without running a destructor, and each Probe owns a
	// std::string. Destroying them explicitly is what real code has to do, and
	// it keeps this suite clean under LeakSanitizer rather than leaving it to
	// depend on the strings being short enough for the small-string buffer.
	for (Probe* probe : { d, e }) { pool.destroy(probe); }
	Check(g_liveProbes == 0, "destroying them by hand is what the arena will not do for you");
}

static void TestArenaPoolHandlesAnUnservableRequest()
{
	std::printf("a pool its arena cannot fund is inert rather than dangerous\n");
	Arena tiny(16);
	ArenaPool<Probe> pool(tiny, 1000);

	Check(!pool.valid(), "the pool reports itself invalid");
	Check(pool.capacity() == 0, "with no slots");
	Check(pool.try_construct("x", 1) == nullptr, "and hands out nothing");

	Check(ArenaPool<Probe>::bytesFor(0) == 0, "bytesFor(0) is zero");
	Check(ArenaPool<Probe>::bytesFor(SIZE_MAX) == 0, "and an overflowing count is refused");
}

/** SlotMap **/

static void TestSlotMapDetectsStaleHandles()
{
	std::printf("a handle to a destroyed object does not resolve to its replacement\n");
	SlotMap<int> map;

	const auto alice = map.emplace(1);
	Check(map.get(alice) != nullptr && *map.get(alice) == 1, "the handle resolves while the object lives");

	Check(map.erase(alice), "erase reports that it removed something");
	Check(map.get(alice) == nullptr, "and the handle stops resolving");
	Check(!map.contains(alice), "contains() agrees");
	Check(!map.erase(alice), "erasing twice is harmless and reports nothing removed");

	// The ABA case this type exists for: the slot is reused, and the old
	// handle must not quietly start naming the new occupant.
	const auto bob = map.emplace(2);
	Check(bob.index == alice.index, "the freed slot really is reused");
	Check(bob.generation != alice.generation, "with a new generation");
	Check(map.get(alice) == nullptr, "so the old handle still resolves to nothing");
	Check(map.get(bob) != nullptr && *map.get(bob) == 2, "while the new one finds the new object");
}

static void TestSlotMapNullHandleNeverResolves()
{
	std::printf("a default-constructed handle refers to nothing, even at index zero\n");
	SlotMap<int> map;

	SlotMap<int>::Handle none;
	Check(!none.valid() && !static_cast<bool>(none), "a default handle reports itself invalid");
	Check(map.get(none) == nullptr, "and resolves to nothing on an empty map");

	const auto first = map.emplace(42);
	Check(first.index == 0, "the first object takes slot zero");
	Check(first.generation != 0, "but never generation zero, which is reserved for null");
	Check(map.get(none) == nullptr, "so a zeroed handle cannot alias the first object");
}

static void TestSlotMapKeepsStorageDense()
{
	std::printf("live objects stay packed, and erasing fixes up the moved object's handle\n");
	SlotMap<int> map;

	const auto a = map.emplace(10);
	const auto b = map.emplace(20);
	const auto c = map.emplace(30);
	Check(map.size() == 3, "three objects are live");

	// Contiguous: the whole reason to prefer this over a node-based map.
	Check(map.data() != nullptr, "the dense array exists");
	Check(&map.data()[2] - &map.data()[0] == 2, "and its elements are adjacent");

	// Erasing the first moves the last into the hole. The moved object's
	// handle must follow it.
	Check(map.erase(a), "the first object is erased");
	Check(map.size() == 2, "leaving two");
	Check(map.get(a) == nullptr, "its handle is stale");
	Check(map.get(b) != nullptr && *map.get(b) == 20, "the untouched object still resolves");
	Check(map.get(c) != nullptr && *map.get(c) == 30, "and so does the one that was moved into the hole");

	int sum = 0;
	for (int value : map) { sum += value; }
	Check(sum == 50, "iterating the dense array visits exactly the live objects");
}

static void TestSlotMapHandleAtMatchesDenseOrder()
{
	std::printf("handleAt() recovers a durable handle from a position in the dense array\n");
	SlotMap<int> map;
	map.emplace(1);
	const auto second = map.emplace(2);
	map.emplace(3);
	map.erase(second);   // force a swap so positions no longer match slots

	bool allMatch = true;
	for (std::size_t i = 0; i < map.size(); ++i)
	{
		const auto handle = map.handleAt(i);
		if (map.get(handle) != &map.data()[i]) { allMatch = false; }
	}
	Check(allMatch, "every position resolves back to itself");
	Check(!map.handleAt(map.size()).valid(), "a position past the end yields a null handle");
}

static void TestSlotMapClearInvalidatesEveryHandle()
{
	std::printf("clear() destroys everything and leaves no handle resolving\n");
	SlotMap<int> map;
	const auto a = map.emplace(1);
	const auto b = map.emplace(2);

	map.clear();
	Check(map.empty() && map.size() == 0, "the map is empty");
	Check(map.get(a) == nullptr && map.get(b) == nullptr, "and neither handle resolves");

	const auto fresh = map.emplace(3);
	Check(map.get(fresh) != nullptr && *map.get(fresh) == 3, "new objects work afterwards");
	Check(map.get(a) == nullptr, "and the pre-clear handles stay stale");
}

static void TestSlotMapHandlePacking()
{
	std::printf("a handle survives a round trip through its packed form\n");
	SlotMap<int> map;
	const auto handle = map.emplace(7);

	const std::uint64_t packed = handle.bits();
	const auto restored = SlotMap<int>::Handle::fromBits(packed);

	Check(restored == handle, "the unpacked handle equals the original");
	Check(map.get(restored) != nullptr && *map.get(restored) == 7, "and still resolves");
}

static void TestSlotMapSurvivesChurn()
{
	std::printf("handles stay correct across sustained spawn/despawn churn\n");
	SlotMap<int> map;
	std::vector<SlotMap<int>::Handle> handles;
	std::vector<int> expected;

	for (int i = 0; i < 200; ++i)
	{
		handles.push_back(map.emplace(i));
		expected.push_back(i);
	}

	// Erase every third, which forces repeated swap-removes and slot reuse.
	for (std::size_t i = 0; i < handles.size(); i += 3)
	{
		map.erase(handles[i]);
		expected[i] = -1;
	}
	for (int i = 200; i < 260; ++i)
	{
		handles.push_back(map.emplace(i));
		expected.push_back(i);
	}

	bool correct = true;
	for (std::size_t i = 0; i < handles.size(); ++i)
	{
		const int* value = map.get(handles[i]);
		if (expected[i] < 0)
		{
			if (value != nullptr) { correct = false; }
		}
		else if (value == nullptr || *value != expected[i]) { correct = false; }
	}
	Check(correct, "every handle resolves to its own object, or to nothing if erased");

	std::size_t liveExpected = 0;
	for (int value : expected) { if (value >= 0) { ++liveExpected; } }
	Check(map.size() == liveExpected, "and the live count matches");
}

/** MemoryTracker **/

static void TestMemoryTrackerAccountsByTag()
{
	std::printf("the tracker totals bytes per tag and remembers the peak\n");
	MemoryTracker tracker;

	tracker.add(MemoryTag::Level, 1024);
	tracker.add(MemoryTag::Level, 2048);
	tracker.add(MemoryTag::Particles, 512);

	Check(tracker.live(MemoryTag::Level) == 3072, "live bytes add up within a tag");
	Check(tracker.blocks(MemoryTag::Level) == 2, "blocks counts the registrations");
	Check(tracker.live(MemoryTag::Particles) == 512, "tags are independent");
	Check(tracker.liveTotal() == 3584, "and the total spans them");

	tracker.remove(MemoryTag::Level, 2048);
	Check(tracker.live(MemoryTag::Level) == 1024, "removing a block subtracts it");
	Check(tracker.peak(MemoryTag::Level) == 3072, "but the peak remembers the busiest moment");

	// A mismatched remove must not wrap the counter into a number that makes
	// every later reading meaningless.
	tracker.remove(MemoryTag::Level, 99999);
	Check(tracker.live(MemoryTag::Level) == 0, "an oversized remove clamps at zero");
}

static void TestMemoryTrackerBudgets()
{
	std::printf("budgets report when a tag goes over\n");
	MemoryTracker tracker;

	Check(!tracker.overBudget(MemoryTag::Audio), "an unbudgeted tag is never over");

	tracker.setBudget(MemoryTag::Audio, 1024);
	tracker.add(MemoryTag::Audio, 512);
	Check(!tracker.overBudget(MemoryTag::Audio), "under the budget is not over it");

	tracker.add(MemoryTag::Audio, 1024);
	Check(tracker.overBudget(MemoryTag::Audio), "past the budget is");

	const std::string report = tracker.report();
	Check(report.find("Audio") != std::string::npos, "the report names the tag");
	Check(report.find("OVER") != std::string::npos, "and flags it");
	Check(report.find("Physics") == std::string::npos, "while leaving out tags nothing ever used");
}

static void TestMemoryTrackerRegistrationIsBalanced()
{
	std::printf("Registration reports a block and gives it back on destruction\n");
	MemoryTracker tracker;

	{
		MemoryTracker::Registration reg(tracker, MemoryTag::Entities, 4096);
		Check(reg.active() && reg.bytes() == 4096, "the registration holds its block");
		Check(tracker.live(MemoryTag::Entities) == 4096, "which the tracker sees");
	}
	Check(tracker.live(MemoryTag::Entities) == 0, "and gives back when it goes out of scope");
	Check(tracker.peak(MemoryTag::Entities) == 4096, "leaving the peak behind");

	{
		MemoryTracker::Registration reg(tracker, MemoryTag::UI, 256);
		reg.release();
		Check(!reg.active() && tracker.live(MemoryTag::UI) == 0, "release() gives it back early");
		reg.release();
		Check(tracker.live(MemoryTag::UI) == 0, "and releasing twice does not double-subtract");
	}

	// Moving must transfer the obligation, not duplicate or drop it.
	{
		MemoryTracker::Registration outer;
		{
			MemoryTracker::Registration inner(tracker, MemoryTag::Assets, 128);
			outer = std::move(inner);
		}
		Check(tracker.live(MemoryTag::Assets) == 128, "a moved-from registration gives nothing back");
	}
	Check(tracker.live(MemoryTag::Assets) == 0, "the block is released once, by the destination");
}

int main()
{
	std::printf("Allocator tests\n\n");

	TestArenaHandsOutDistinctAlignedBlocks();
	TestArenaOverAlignedRequest();
	TestArenaExhaustionReturnsNullRatherThanThrowing();
	TestArenaResetReusesTheSameMemory();
	TestArenaHighWaterSurvivesReset();

	TestArenaRewindReleasesBackToMarker();
	TestArenaRewindSpansAlignmentPadding();
	TestArenaRewindIgnoresStaleMarkers();
	TestArenaScopeRewindsOnExit();
	TestArenaTypedConstruction();

	TestVirtualArenaCommitsOnDemand();
	TestVirtualArenaKeepsPointersValidAcrossGrowth();
	TestVirtualArenaExhaustsAtTheReservation();
	TestVirtualArenaOverAlignedRequest();
	TestVirtualArenaResetKeepsPagesDecommitReturnsThem();
	TestVirtualArenaMarkers();

	TestFrameAllocatorKeepsLastFramesData();
	TestFrameAllocatorRecyclesTwoBuffers();
	TestArenaResourceBacksStandardContainers();

	TestPoolConstructsAndDestroys();
	TestPoolRecyclesFreedSlots();
	TestPoolExhaustionThrows();
	TestPoolStrideHandlesAwkwardSizes();
	TestPoolReportsOccupancy();
	TestPoolTryConstructReturnsNullWhenFull();
	TestArenaPoolLivesInsideAnArena();
	TestArenaPoolHandlesAnUnservableRequest();

	TestSlotMapDetectsStaleHandles();
	TestSlotMapNullHandleNeverResolves();
	TestSlotMapKeepsStorageDense();
	TestSlotMapHandleAtMatchesDenseOrder();
	TestSlotMapClearInvalidatesEveryHandle();
	TestSlotMapHandlePacking();
	TestSlotMapSurvivesChurn();

	TestMemoryTrackerAccountsByTag();
	TestMemoryTrackerBudgets();
	TestMemoryTrackerRegistrationIsBalanced();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
