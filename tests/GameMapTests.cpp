/**
 * Behavioural tests for GameMap's Spawn/Destroy/Find/ForEach interface.
 *
 * GameMap used to expose gameObjects/entities/interactables directly, and
 * gameObjects was a std::vector<GameObject> — a push_back could reallocate
 * and invalidate every pointer callers had already taken out. The container
 * is now an id-keyed std::unordered_map<uint64_t, GameObject>, which
 * guarantees element addresses survive further insertions; that guarantee,
 * not just the rename, is what these tests hold the line on.
 *
 * Same tier as StalkerFsmTests.cpp: needs the engine (GameObject's
 * constructor uploads a fallback cube, which needs a GL context), so a
 * window is opened and this links against the game's own object files. See
 * tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <gameMap.h>

#include <algorithm>
#include <cstdio>
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

// Scene_new() (rather than a bare `new Scene`) so scene->player exists —
// DestroyInteractable reaches SceneManager::currentScene->player directly.
static Scene* MakeScene()
{
	return Scene_new();
}

static void TestIdsUniqueAcrossKinds()
{
	std::printf("ids stay unique across GameObject/Entity/Interactable, sharing one counter\n");
	Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	std::vector<std::uint64_t> ids;
	for (int i = 0; i < 5; ++i)
	{
		GameObject go{};
		ids.push_back(map.SpawnGameObject(go)->id);

		Entity entity{};
		ids.push_back(map.SpawnEntity(entity)->id);

		InteractableObject interactable(INTERACT_ITEM, 0);
		ids.push_back(map.SpawnInteractable(interactable)->id);
	}

	std::sort(ids.begin(), ids.end());
	Check(std::adjacent_find(ids.begin(), ids.end()) == ids.end(),
		"every spawned object across all three kinds gets a distinct id");

	delete scene;
}

static void TestFindPointerSurvivesFurtherSpawns()
{
	std::printf("a Find pointer survives further spawns (regression: gameObjects was a reallocating vector)\n");
	Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	GameObject first{};
	first.name = "First";
	GameObject* spawned = map.SpawnGameObject(first);
	const std::uint64_t firstId = spawned->id;

	// Plenty more than any small-vector/bucket-count threshold, so this would
	// have reallocated the old std::vector<GameObject> at least once.
	for (int i = 0; i < 200; ++i)
	{
		GameObject other{};
		map.SpawnGameObject(other);
	}

	GameObject* found = map.FindGameObject(firstId);
	Check(found == spawned, "the pointer handed back at spawn time is still the pointer Find returns");
	Check(found != nullptr && found->name == "First", "the object's data survived unmoved");

	delete scene;
}

static void TestDestroyInteractableScrubsPlayerInventory()
{
	std::printf("destroying an interactable scrubs the player's inventory and interactObjectId\n");
	Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	InteractableObject item(INTERACT_ITEM, 0);
	InteractableObject* spawned = map.SpawnInteractable(item);

	scene->player->inventory.push_back(spawned);
	scene->player->interactObjectId = spawned->id;

	map.DestroyInteractable(spawned->id);

	Check(std::find(scene->player->inventory.begin(), scene->player->inventory.end(), spawned)
			== scene->player->inventory.end(),
		"the destroyed interactable is removed from the player's inventory");
	Check(scene->player->interactObjectId == 0,
		"the player's interactObjectId is cleared when it pointed at the destroyed interactable");
	Check(map.FindInteractable(spawned->id) == nullptr, "the interactable itself is gone");

	delete scene;
}

static void TestForEachGameObjectVisitsAllAndHonoursEarlyExit()
{
	std::printf("ForEachGameObject visits every live object and stops early on a false return\n");
	Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	for (int i = 0; i < 5; ++i)
	{
		GameObject go{};
		map.SpawnGameObject(go);
	}

	int visited = 0;
	map.ForEachGameObject([&](GameObject&) { ++visited; });
	Check(visited == 5, "a void visitor visits every live gameObject");

	int visitedBeforeStop = 0;
	map.ForEachGameObject([&](GameObject&) -> bool {
		++visitedBeforeStop;
		return visitedBeforeStop < 2; // stop after the second visit
	});
	Check(visitedBeforeStop == 2, "a bool visitor returning false stops the traversal early");

	delete scene;
}

static void TestForEachObjectPairVisitsEveryUnorderedPairOnce()
{
	std::printf("ForEachObjectPair visits every unordered pair of live objects exactly once\n");
	Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	constexpr int kCount = 4;
	for (int i = 0; i < kCount; ++i)
	{
		GameObject go{};
		map.SpawnGameObject(go);
	}

	int pairsVisited = 0;
	map.ForEachObjectPair([&](GameObject&, GameObject&) { ++pairsVisited; });
	Check(pairsVisited == (kCount * (kCount - 1)) / 2, "every unordered pair is visited exactly once");

	int pairsBeforeStop = 0;
	map.ForEachObjectPair([&](GameObject&, GameObject&) -> bool {
		++pairsBeforeStop;
		return false; // stop immediately
	});
	Check(pairsBeforeStop == 1, "a bool visitor returning false stops pair traversal after the first pair");

	delete scene;
}

int main()
{
	// GameObject's constructor uploads a fallback cube, so a GL context has to
	// exist before any object is spawned — see StalkerFsmTests.cpp.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-gamemap-tests");

	std::printf("GameMap tests\n\n");

	TestIdsUniqueAcrossKinds();
	TestFindPointerSurvivesFurtherSpawns();
	TestDestroyInteractableScrubsPlayerInventory();
	TestForEachGameObjectVisitsAllAndHonoursEarlyExit();
	TestForEachObjectPairVisitsEveryUnorderedPairOnce();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
