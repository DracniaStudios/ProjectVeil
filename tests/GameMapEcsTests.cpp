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

int main()
{
	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(64, 64, "veil-gamemap-ecs-tests");

	std::printf("GameMap on EnTT tests\n\n");

	TestPointersSurviveChurn();
	TestIdsAndKinds();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
