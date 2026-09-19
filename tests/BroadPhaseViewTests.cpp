/**
 * Behavioural tests for BroadPhaseView.
 *
 * The view exists to make the collision broad phase cheaper, and a faster solver
 * that resolves a different set of pairs is not faster, it is broken. So the
 * property under test is equality, not similarity: for a randomised world, the
 * pairs the packed view accepts must be exactly the pairs the container walk
 * accepts. Not a superset -- the view is the same O(n^2) traversal, so anything
 * other than an exact match is a bug.
 *
 * Needs the engine: GameObject's constructor uploads a fallback cube, so a GL
 * context has to exist before anything is spawned. Same tier as GameMapTests.
 *
 * Build and run: tests/run_tests.sh
 */

#include <raylib.h>

#include <ArenaAllocator.h>
#include <BroadPhaseView.h>
#include <Scene.h>
#include <SceneManager.h>
#include <gameMap.h>

#include <algorithm>
#include <cstdio>
#include <random>
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

using Pair = std::pair<std::uint64_t, std::uint64_t>;

static Pair MakePair(std::uint64_t a, std::uint64_t b)
{
	return a < b ? Pair{ a, b } : Pair{ b, a };
}

// The gate exactly as Scene.cpp's skipCollisionPair applies it. Duplicated here
// on purpose: if the two ever diverge, this suite is what notices.
static bool SkipByBody(const RigidBody3D& a, const RigidBody3D& b)
{
	if (!a.collider.canCollide || !b.collider.canCollide) { return true; }
	if (a.collider.isTrigger() || b.collider.isTrigger()) { return false; }
	return a.isStatic && b.isStatic;
}

// Reference implementation: walk the containers, collect every unordered pair
// that passes the gate and the box test. The union of the solver's six
// map-side passes is exactly every unordered pair across the three containers.
static std::vector<Pair> PairsByContainers(GameMap& map)
{
	std::vector<GameObject*> all;
	map.ForEachObject([&](GameObject& o) { all.push_back(&o); });

	std::vector<Pair> pairs;
	for (std::size_t i = 0; i < all.size(); ++i)
	{
		for (std::size_t j = i + 1; j < all.size(); ++j)
		{
			if (SkipByBody(all[i]->rigidBody3D, all[j]->rigidBody3D)) { continue; }
			if (!all[i]->rigidBody3D.OverlapsBroadPhase(all[j]->rigidBody3D)) { continue; }
			pairs.push_back(MakePair(all[i]->id, all[j]->id));
		}
	}
	std::sort(pairs.begin(), pairs.end());
	return pairs;
}

static std::vector<Pair> PairsByView(const BroadPhaseView& view)
{
	std::vector<Pair> pairs;
	for (std::size_t i = 0; i < view.size(); ++i)
	{
		for (std::size_t j = i + 1; j < view.size(); ++j)
		{
			if (BroadPhaseView::SkipPair(view.rec(i), view.rec(j))) { continue; }
			if (!BroadPhaseView::Overlaps(view.rec(i), view.rec(j))) { continue; }
			pairs.push_back(MakePair(view.owner(i)->id, view.owner(j)->id));
		}
	}
	std::sort(pairs.begin(), pairs.end());
	return pairs;
}

// Spawns a world with enough overlap, staticness and trigger variety that the
// gate's branches are all exercised rather than just its early-out.
static void PopulateRandomWorld(GameMap& map, unsigned seed, int objects, int entities, int interactables)
{
	std::mt19937 rng(seed);
	std::uniform_real_distribution<float> pos(-6.0f, 6.0f);
	std::uniform_int_distribution<int> coin(0, 3);

	auto configure = [&](GameObject& o)
	{
		o.rigidBody3D.Teleport(Vector3{ pos(rng), pos(rng), pos(rng) });
		o.rigidBody3D.scale = Vector3{ 2.0f, 2.0f, 2.0f };
		o.rigidBody3D.isStatic = coin(rng) != 0;                 // mostly static, like a real world
		o.rigidBody3D.collider.canCollide = coin(rng) != 0;      // some bodies opted out
		o.rigidBody3D.collider.mode = coin(rng) == 0 ? COLLIDER_TRIGGER : COLLIDER_COLLISION;
		o.rigidBody3D.SyncBroadPhaseBox();
	};

	for (int i = 0; i < objects; ++i)
	{
		GameObject o{};
		configure(o);
		GameObject* spawned = map.SpawnGameObject(o);
		configure(*spawned);   // onEnable() in Spawn* can rebuild the box; settle it after
	}
	for (int i = 0; i < entities; ++i)
	{
		Entity e{};
		configure(e);
		Entity* spawned = map.SpawnEntity(e);
		configure(*spawned);
	}
	for (int i = 0; i < interactables; ++i)
	{
		InteractableObject it(INTERACT_ITEM, 0);
		configure(it);
		InteractableObject* spawned = map.SpawnInteractable(it);
		configure(*spawned);
	}
}

static void TestViewMatchesContainerWalk()
{
	std::printf("the packed view accepts exactly the pairs the container walk accepts\n");

	for (unsigned seed = 1; seed <= 5; ++seed)
	{
		Scene* scene = Scene_new();
		GameMap& map = scene->gameMap;
		PopulateRandomWorld(map, seed, 24, 3, 5);

		Arena arena(64 * 1024);
		BroadPhaseView view;
		Check(view.Build(map, arena), "Build succeeds with room to spare");
		Check(view.size() == map.GameObjectCount() + map.EntityCount() + map.InteractableCount(),
			"the view holds one record per body in the map");

		const std::vector<Pair> reference = PairsByContainers(map);
		const std::vector<Pair> fromView = PairsByView(view);

		Check(fromView == reference,
			"seed " + std::to_string(seed) + ": the pair sets are identical");

		// A world where nothing overlaps would pass the comparison trivially, so
		// make sure the fixture is actually producing work.
		if (seed == 1)
		{
			Check(!reference.empty(), "the fixture produces overlapping pairs to compare");
		}

		delete scene;
	}
}

static void TestGroupRangesCoverEveryRecordOnce()
{
	std::printf("the group ranges partition the view exactly\n");
	Scene* scene = Scene_new();
	GameMap& map = scene->gameMap;
	PopulateRandomWorld(map, 99, 7, 2, 3);

	Arena arena(64 * 1024);
	BroadPhaseView view;
	view.Build(map, arena);

	Check(view.groupEnd(BPG_OBJECT) - view.groupBegin(BPG_OBJECT) == map.GameObjectCount(),
		"the object range is the object count");
	Check(view.groupEnd(BPG_ENTITY) - view.groupBegin(BPG_ENTITY) == map.EntityCount(),
		"the entity range is the entity count");
	Check(view.groupEnd(BPG_INTERACTABLE) - view.groupBegin(BPG_INTERACTABLE) == map.InteractableCount(),
		"the interactable range is the interactable count");
	Check(view.groupBegin(BPG_OBJECT) == 0 && view.groupEnd(BPG_INTERACTABLE) == view.size(),
		"the ranges are contiguous and cover the whole view");

	delete scene;
}

static void TestRefreshBoxPicksUpAMove()
{
	std::printf("RefreshBox re-reads the owner's box after it moves\n");
	Scene* scene = Scene_new();
	GameMap& map = scene->gameMap;
	PopulateRandomWorld(map, 7, 4, 0, 0);

	Arena arena(64 * 1024);
	BroadPhaseView view;
	view.Build(map, arena);

	GameObject* owner = view.owner(0);
	const BoundingBox before = view.rec(0).box;

	owner->rigidBody3D.Teleport(Vector3{ 500.0f, 500.0f, 500.0f });
	owner->rigidBody3D.SyncBroadPhaseBox();

	Check(view.rec(0).box.min.x == before.min.x,
		"the record is stale until it is refreshed -- that is why the solver writes back");

	view.RefreshBox(0);
	Check(view.rec(0).box.min.x == owner->rigidBody3D.broadPhaseBox.min.x,
		"RefreshBox brings the record back in step with its owner");

	delete scene;
}

static void TestExhaustedArenaFailsCleanly()
{
	std::printf("a too-small arena fails the build rather than throwing or overrunning\n");
	Scene* scene = Scene_new();
	GameMap& map = scene->gameMap;
	PopulateRandomWorld(map, 3, 40, 0, 0);

	Arena tiny(64);   // nowhere near enough
	BroadPhaseView view;
	Check(!view.Build(map, tiny), "Build reports failure");
	Check(!view.valid(), "and leaves the view unusable, so the caller falls back");

	delete scene;
}

int main()
{
	// GameObject's constructor uploads a fallback cube, so a GL context has to
	// exist before any object is spawned -- see GameMapTests.cpp.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-broadphase-tests");

	std::printf("BroadPhaseView tests\n\n");

	TestViewMatchesContainerWalk();
	TestGroupRangesCoverEveryRecordOnce();
	TestRefreshBoxPicksUpAMove();
	TestExhaustedArenaFailsCleanly();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
