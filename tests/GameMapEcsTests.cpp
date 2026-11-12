/**
 * GameMap on EnTT (MultiplayerSplitPlan.md E5, rules E-1, E-2): the storage
 * changed, the contract didn't.
 *
 *   - A pointer to a stored object stays valid while others are spawned and
 *     destroyed around it (in-place deletion; the node-based maps gave this).
 *   - Ids are the World save's: restored objects keep theirs, spawns continue
 *     above them, and every kind shares the one id space.
 *   - ForEach* sees exactly the live objects of its kind; counts agree.
 *   - Clear empties the registry and restarts the ids.
 *   - Traversal order is pinned (ADR C item 5).
 */

#include <raylib.h>

#include <Scene.h>
#include <AI/Stalker.h>

#include <cstdio>
#include <set>
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

static void TestPointersSurviveChurn()
{
	std::printf("a stored object's pointer survives spawns and removals around it\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	GameMap& map = scene->gameMap;

	GameObject prototype;
	prototype.name = "Keep";
	GameObject* keep = map.SpawnGameObject(prototype);
	const std::uint64_t keepId = keep->id;

	InteractableObject stationPrototype(INTERACT_MINIGAME, 0, 0);
	InteractableObject* station = map.SpawnInteractable(stationPrototype);

	// Lots of churn: hundreds of spawns, half of them destroyed and swept
	std::vector<std::uint64_t> spawned;
	for (int i = 0; i < 600; ++i)
	{
		GameObject other;
		other.name = "Churn";
		spawned.push_back(map.SpawnGameObject(other)->id);
	}
	for (std::size_t i = 0; i < spawned.size(); i += 2) { map.Destroy(spawned[i]); }
	map.SweepDestroyed(scene);
	for (int i = 0; i < 300; ++i) { GameObject more; map.SpawnGameObject(more); }

	Check(map.FindGameObject(keepId) == keep, "the kept GameObject is at the same address after 900 spawns and 300 removals");
	Check(keep->name == "Keep" && keep->id == keepId, "and still holds its own data");
	Check(map.FindInteractable(station->id) == station, "an Interactable's pointer is stable too");
	Check(map.GameObjectCount() == 1 + 600 - 300 + 300, "counts only live objects (" + std::to_string(map.GameObjectCount()) + ")");

	int visited = 0;
	map.ForEachGameObject([&](GameObject& o) { ++visited; Check(!o.pendingDestroy, "no swept object is visited"); return visited < 5; });
	Check(visited == 5, "a visitor returning false stops the traversal");

	delete scene;
}

static void TestIdsAndKinds()
{
	std::printf("ids are the World save's and kinds stay apart\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	GameMap& map = scene->gameMap;

	GameObject wall;
	wall.id = 40;
	map.RestoreGameObject(wall);
	auto stalker = std::make_unique<Stalker>();
	stalker->id = 41;
	map.RestoreEntity(std::move(stalker));
	auto station = std::make_unique<InteractableObject>(INTERACT_MINIGAME, 0, 0);
	station->id = 42;
	map.RestoreInteractable(std::move(station));

	Check(map.FindGameObject(40) && map.FindEntity(41) && map.FindInteractable(42), "each restored object is found under its own id");
	Check(!map.FindEntity(40) && !map.FindGameObject(41) && !map.FindInteractable(40), "and only as its own kind");
	Check(map.FindWorldObject(41) == map.FindEntity(41), "FindWorldObject finds any kind");

	GameObject clash;
	clash.id = 41;
	Check(map.RestoreGameObject(clash) == nullptr, "an id already used by another kind is refused");
	Check(map.NextId() == 43, "the next spawn goes above the restored ids");
	GameObject fresh;
	Check(map.SpawnGameObject(fresh)->id == 43, "and gets that id");

	std::set<std::uint64_t> seen;
	map.ForEachObject([&](GameObject& o) { seen.insert(o.id); });
	Check(seen == std::set<std::uint64_t>{ 40, 41, 42, 43 }, "ForEachObject sees every kind once");

	map.Clear();
	Check(map.GameObjectCount() == 0 && map.EntityCount() == 0 && map.InteractableCount() == 0, "Clear empties the registry");
	Check(map.NextId() == FIRST_OBJECT_ID && !map.FindGameObject(40), "and restarts the ids");
	delete scene;
}

static std::string FormatIds(const std::vector<std::uint64_t>& ids)
{
	std::string text = "{";
	for (std::size_t i = 0; i < ids.size(); ++i)
	{
		text += (i == 0 ? " " : ", ") + std::to_string(ids[i]);
	}
	return text + " }";
}

static void CheckOrder(const std::vector<std::uint64_t>& visited, const std::vector<std::uint64_t>& expected, const std::string& what)
{
	Check(visited == expected, what + " (visited " + FormatIds(visited) + ", expected " + FormatIds(expected) + ")");
}

// Traversal order is part of the contract (ADR C item 5): the newest slot of a
// kind comes first, a freed slot is reused last-freed-first, and ForEachObject
// goes GameObjects, then Entities, then Interactables. Derived from EnTT 4.0.0's
// sparse_set.hpp; if one of these ever fails on unchanged code, the visited ids
// in the message are the characterization.
static void TestTraversalOrderIsPinned()
{
	std::printf("traversal order is pinned: newest slot first, freed slots reused last-freed-first\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	GameMap& map = scene->gameMap;

	const auto gameObjectOrder = [&map]
	{
		std::vector<std::uint64_t> ids;
		map.ForEachGameObject([&](GameObject& o) { ids.push_back(o.id); });
		return ids;
	};
	const auto objectOrder = [&map]
	{
		std::vector<std::uint64_t> ids;
		map.ForEachObject([&](GameObject& o) { ids.push_back(o.id); });
		return ids;
	};

	// (a) spawn four, destroy two, spawn three into the freed slots
	std::vector<std::uint64_t> firstIds;
	for (int i = 0; i < 4; ++i) { GameObject o; firstIds.push_back(map.SpawnGameObject(o)->id); }
	CheckOrder(firstIds, { 2, 3, 4, 5 }, "four plain spawns get ids 2 to 5");
	map.Destroy(3);
	map.Destroy(5);
	map.SweepDestroyed(scene);
	std::vector<std::uint64_t> laterIds;
	for (int i = 0; i < 3; ++i) { GameObject o; laterIds.push_back(map.SpawnGameObject(o)->id); }
	CheckOrder(laterIds, { 6, 7, 8 }, "three more spawns get ids 6 to 8");
	CheckOrder(gameObjectOrder(), { 8, 7, 4, 6, 2 }, "(a) ForEachGameObject after destroy, sweep and respawn");

	// (b) restored ids need not be ascending
	map.Clear();
	for (const std::uint64_t id : { 40ull, 100ull, 7ull })
	{
		GameObject o;
		o.id = id;
		map.RestoreGameObject(o);
	}
	CheckOrder(gameObjectOrder(), { 7, 100, 40 }, "(b) ForEachGameObject after Restore of 40, 100, 7");

	// (c) one of each kind: GameObjects, then Entities, then Interactables
	map.Clear();
	{
		GameObject plain;
		Entity entity;
		InteractableObject interactable(INTERACT_ITEM, 0);
		std::vector<std::uint64_t> ids;
		ids.push_back(map.SpawnGameObject(plain)->id);
		ids.push_back(map.SpawnEntity(entity)->id);
		ids.push_back(map.SpawnInteractable(interactable)->id);
		ids.push_back(map.SpawnGameObject(plain)->id);
		CheckOrder(ids, { 2, 3, 4, 5 }, "(c) the four spawns get ids 2 to 5");
	}
	CheckOrder(objectOrder(), { 5, 2, 3, 4 }, "(c) ForEachObject visits GameObjects, Entities, then Interactables");

	delete scene;
}

int main()
{
	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(64, 64, "veil-gamemap-ecs-tests");

	std::printf("GameMap on EnTT tests\n\n");

	TestPointersSurviveChurn();
	TestIdsAndKinds();
	TestTraversalOrderIsPinned();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
