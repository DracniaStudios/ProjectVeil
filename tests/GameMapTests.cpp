/**
 * Behavioural tests for GameMap's lifecycle: Spawn, Restore, Destroy, Clear,
 * plus Find and ForEach.
 *
 * Every kind of object (GameObject, Entity, InteractableObject) goes through
 * the same four calls with the same rules:
 *
 *   Spawn    stores its own copy of a prototype under a fresh id and enables it
 *   Restore  puts a saved object back under its own id, as it was saved
 *   Destroy  marks an object; SweepDestroyed removes it at the end of the frame
 *   Clear    removes everything now and restarts the ids
 *
 * The containers are id-keyed std::unordered_maps, so element addresses
 * survive further insertions; TestFindPointerSurvivesFurtherSpawns holds the
 * line on that.
 *
 * Same tier as StalkerFsmTests.cpp: needs the engine (Spawn and Restore bind a
 * model, which needs a GL context), so a window is opened and this links
 * against the game's own object files. See tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <gameMap.h>

#include <algorithm>
#include <cstdio>
#include <memory>
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

// Scene_new() (rather than a bare `new Scene`) so scene->player exists, which
// item pickups and Unlock hand items and the Artifact to.
static SceneManagement::Scene* MakeScene()
{
	return SceneManagement::Scene_new();
}

// ─── Spawn ─────────────────────────────────────────────────────────────────

static void TestIdsUniqueAcrossKinds()
{
	std::printf("ids stay unique across GameObject/Entity/Interactable, sharing one counter\n");
	SceneManagement::Scene* scene = MakeScene();
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
	Check(ids.front() >= FIRST_OBJECT_ID, "no spawned object takes the no-object or player id");

	delete scene;
}

static void TestFindPointerSurvivesFurtherSpawns()
{
	std::printf("a Find pointer survives further spawns (regression: gameObjects was a reallocating vector)\n");
	SceneManagement::Scene* scene = MakeScene();
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

static void TestSpawnEntityKeysByNewIdAndDoesNotClobberSiblings()
{
	std::printf("SpawnEntity keys the map by the id it assigns, and a second spawn "
		"does not destroy the first (regression: both defaulted to id 0 before "
		"their real id was assigned, so the second spawn's map insert under the "
		"stale key overwrote and destroyed the first entity)\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	Entity first{};
	Entity* spawnedFirst = map.SpawnEntity(first);
	const std::uint64_t firstId = spawnedFirst->id;

	Entity second{};
	Entity* spawnedSecond = map.SpawnEntity(second);
	const std::uint64_t secondId = spawnedSecond->id;

	Check(firstId != secondId, "the two entities got distinct ids");
	Check(map.FindEntity(firstId) == spawnedFirst,
		"the first entity is still alive and reachable by its own id");
	Check(map.FindEntity(secondId) == spawnedSecond,
		"the second entity is reachable by its own id");

	// Also exercises the DuplicateSelection path: cloning an entity that
	// already carries a real (non-zero) id and spawning the clone must not
	// destroy the original the id was copied from.
	Entity* duplicate = map.SpawnEntity(*spawnedFirst);
	Check(duplicate->id != firstId, "duplicating an already-spawned entity gets its own new id");
	Check(map.FindEntity(firstId) == spawnedFirst,
		"the original entity survives being duplicated");
	Check(map.FindEntity(duplicate->id) == duplicate,
		"the duplicate is reachable by its own id");

	delete scene;
}

static void TestANewObjectHoldsNoModel()
{
	std::printf("a new GameObject holds no GPU model until it is spawned or restored\n");

	GameObject fresh{};
	Check(fresh.model.meshCount == 0, "the constructor uploads no mesh");
	Check(!fresh.ownsModel, "so there is nothing for it to own");

	Entity entity{};
	Check(entity.model.meshCount == 0, "neither does an Entity's");

	InteractableObject interactable(INTERACT_ITEM, 0);
	Check(interactable.model.meshCount == 0, "nor an Interactable's");
}

static void TestSpawnLeavesThePrototypesModelAlone()
{
	std::printf("Spawn binds the stored copy's own model and never frees the prototype's "
		"(regression: Spawn enabled the prototype and then copied it, so two objects "
		"owned one model, and the editor had to disown or release by hand)\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	// A prototype that is drawn and kept, like the editor's selection when it is
	// duplicated: it owns a generated cube of its own.
	GameObject prototype{};
	prototype.loadVisuals();
	const Mesh* prototypeMeshes = prototype.model.meshes;
	Check(prototype.ownsModel && prototype.model.meshCount > 0, "the prototype owns a model of its own");

	GameObject* first = map.SpawnGameObject(prototype);
	GameObject* second = map.SpawnGameObject(prototype);

	Check(first->ownsModel && first->model.meshCount > 0, "the stored copy has a model");
	Check(first->model.meshes != prototypeMeshes, "and it is not the prototype's");
	Check(first->model.meshes != second->model.meshes, "two spawns from one prototype get a model each");
	Check(prototype.ownsModel && prototype.model.meshes == prototypeMeshes,
		"the prototype still owns the model it had");

	map.Destroy(first->id);
	map.SweepDestroyed(scene);
	Check(prototype.model.meshes == prototypeMeshes && prototype.model.meshCount > 0,
		"destroying a spawned copy leaves the prototype's model alone");

	prototype.releaseGeneratedModel();
	delete scene;
}

static void TestSpawnGivesEveryKindAUnitScale()
{
	std::printf("Spawn gives an object with no scale a unit scale, whatever its kind "
		"(regression: SpawnInteractable skipped it)\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	GameObject go{};
	go.rigidBody3D.scale = Vector3Zero();
	Entity entity{};
	entity.rigidBody3D.scale = Vector3Zero();
	InteractableObject interactable(INTERACT_ITEM, 0);
	interactable.rigidBody3D.scale = Vector3Zero();

	Check(map.SpawnGameObject(go)->rigidBody3D.scale == Vector3One(), "a GameObject");
	Check(map.SpawnEntity(entity)->rigidBody3D.scale == Vector3One(), "an Entity");
	Check(map.SpawnInteractable(interactable)->rigidBody3D.scale == Vector3One(), "an Interactable");

	delete scene;
}

// ─── Restore ───────────────────────────────────────────────────────────────

static void TestRestoreKeepsWhatWasSaved()
{
	std::printf("Restore keeps an object's saved state instead of enabling it "
		"(regression: Load* called onEnable, which re-enabled toggled doors and "
		"healed a damaged Stalker back to full)\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	GameObject door{};
	door.id = 40;
	door.isEnabled = false;
	GameObject* restoredDoor = map.RestoreGameObject(door);
	Check(restoredDoor != nullptr && restoredDoor->id == 40, "the door keeps its id");
	Check(restoredDoor != nullptr && !restoredDoor->isEnabled, "a door saved disabled stays disabled");
	Check(restoredDoor != nullptr && !restoredDoor->rigidBody3D.collider.canCollide,
		"and doesn't block the way while it is disabled");
	Check(restoredDoor != nullptr && restoredDoor->model.meshCount > 0, "its model is bound");

	auto stalker = std::make_unique<Entity>();
	stalker->id = 41;
	stalker->maxHealth = 10.0f;
	stalker->health = 3.0f;
	stalker->stamina = 5.0f;
	Entity* restoredStalker = map.RestoreEntity(std::move(stalker));
	Check(restoredStalker != nullptr && restoredStalker->health == 3.0f, "a damaged entity keeps its health");
	Check(restoredStalker != nullptr && restoredStalker->stamina == 5.0f, "and its stamina");

	auto station = std::make_unique<InteractableObject>(INTERACT_MINIGAME, 0, 0);
	station->id = 42;
	station->isEnabled = false;
	InteractableObject* restoredStation = map.RestoreInteractable(std::move(station));
	Check(restoredStation != nullptr && !restoredStation->isEnabled, "a toggled-off station stays off");

	delete scene;
}

static void TestRestoreRaisesTheNextId()
{
	std::printf("Restore keeps the next spawn's id above every restored one, and a save's "
		"reserved ids hold\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	GameObject saved{};
	saved.id = 500;
	map.RestoreGameObject(saved);
	Check(map.NextId() > 500, "the next id is past the restored one");

	GameObject fresh{};
	Check(map.SpawnGameObject(fresh)->id > 500, "so a spawn after a restore can't collide with it");

	map.ReserveIdsBelow(900);
	Check(map.NextId() >= 900, "ids a save reserved are not handed out");
	map.ReserveIdsBelow(10);
	Check(map.NextId() >= 900, "reserving fewer never lowers the next id");

	delete scene;
}

static void TestRestoreRefusesAnIdInUse()
{
	std::printf("Restore refuses an id another object already holds, of any kind\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	GameObject first{};
	first.id = 60;
	first.name = "First";
	GameObject* kept = map.RestoreGameObject(first);

	auto clash = std::make_unique<Entity>();
	clash->id = 60;
	Check(map.RestoreEntity(std::move(clash)) == nullptr, "a second object under the same id is refused");
	Check(map.FindWorldObject(60) == kept && kept->name == "First", "the first one is untouched");
	Check(map.EntityCount() == 0, "and nothing was added");

	delete scene;
}

// ─── Destroy ───────────────────────────────────────────────────────────────

static void TestDestroyIsDeferredForEveryKind()
{
	std::printf("Destroy marks any kind of object, and SweepDestroyed removes it "
		"(regression: entities and interactables were erased on the spot)\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	GameObject go{};
	Entity entity{};
	InteractableObject interactable(INTERACT_ITEM, 0);
	const std::uint64_t ids[] = {
		map.SpawnGameObject(go)->id,
		map.SpawnEntity(entity)->id,
		map.SpawnInteractable(interactable)->id,
	};

	for (std::uint64_t id : ids) { Check(map.Destroy(id), "Destroy finds the object"); }
	for (std::uint64_t id : ids)
	{
		GameObject* marked = map.FindWorldObject(id);
		Check(marked != nullptr && marked->pendingDestroy, "a destroyed object stays until the sweep");
	}

	map.SweepDestroyed(scene);
	for (std::uint64_t id : ids) { Check(map.FindWorldObject(id) == nullptr, "the sweep removes it"); }
	Check(map.GameObjectCount() == 0 && map.EntityCount() == 0 && map.InteractableCount() == 0,
		"nothing is left behind");

	Check(!map.Destroy(ids[0]), "destroying an id that is gone reports nothing found");

	delete scene;
}

static void TestDestroyInsideForEachIsSafe()
{
	std::printf("destroying objects while visiting them is safe, and every one is still visited\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	for (int i = 0; i < 6; ++i)
	{
		InteractableObject interactable(INTERACT_ITEM, 0);
		map.SpawnInteractable(interactable);
	}

	int visited = 0;
	map.ForEachInteractable([&](InteractableObject& interactable) {
		++visited;
		map.Destroy(interactable.id);
	});
	Check(visited == 6, "the traversal reached every object");

	map.SweepDestroyed(scene);
	Check(map.InteractableCount() == 0, "and the sweep removed them all");

	delete scene;
}

static void TestUnlockRemovesTheStationAtTheSweep()
{
	std::printf("an Unlock station is removed at the end of the frame, not inside its own "
		"onInteract (regression: Unlock erased the station it was running on)\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	InteractableObject unlock(INTERACT_UNLOCK, 0);
	InteractableObject* station = map.SpawnInteractable(unlock);
	const std::uint64_t id = station->id;

	station->onInteract(*scene);
	Check(map.FindInteractable(id) == station, "the station is still there after its own onInteract");
	Check(scene->player->artifact != nullptr, "the Artifact was handed over");

	map.SweepDestroyed(scene);
	Check(map.FindInteractable(id) == nullptr, "the sweep removes the station");

	delete scene->player->artifact;
	scene->player->artifact = nullptr;
	delete scene;
}

static void TestAPickedUpItemIsCarriedById()
{
	std::printf("picking up an item puts its id in the inventory, once\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	InteractableObject pickup(INTERACT_ITEM, 3);
	InteractableObject* item = map.SpawnInteractable(pickup);

	item->onInteract(*scene);
	item->onInteract(*scene);

	Check(scene->player->inventory.size() == 1 && scene->player->inventory.front() == item->id,
		"the inventory holds the item's id, once");
	Check(!item->isEnabled, "the item left the world");

	delete scene;
}

// ─── Clear ─────────────────────────────────────────────────────────────────

static void TestClearEmptiesEveryKindAndRestartsIds()
{
	std::printf("Clear removes every kind of object at once and restarts the ids\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	for (int i = 0; i < 3; ++i)
	{
		GameObject go{};
		map.SpawnGameObject(go);
		Entity entity{};
		map.SpawnEntity(entity);
		InteractableObject interactable(INTERACT_ITEM, 0);
		map.SpawnInteractable(interactable);
	}
	GameObject saved{};
	saved.id = 300;
	map.RestoreGameObject(saved);

	map.Clear();

	Check(map.GameObjectCount() == 0 && map.EntityCount() == 0 && map.InteractableCount() == 0,
		"every container is empty");
	Check(map.NextId() == FIRST_OBJECT_ID, "the next id starts over");

	delete scene;
}

static void TestClearWorldForgetsWhatThePlayerCarries()
{
	std::printf("Scene::ClearWorld empties the map and the player's inventory with it "
		"(the inventory holds ids, which the next world would reuse)\n");
	SceneManagement::Scene* scene = MakeScene();

	InteractableObject pickup(INTERACT_ITEM, 0);
	InteractableObject* item = scene->gameMap.SpawnInteractable(pickup);
	scene->player->inventory.push_back(item->id);

	scene->ClearWorld();

	Check(scene->player->inventory.empty(), "the inventory is empty");
	Check(scene->gameMap.InteractableCount() == 0, "and so is the map");

	delete scene;
}

static void TestCreateReplacesTheWholeWorld()
{
	std::printf("create replaces every kind of object with a single floor "
		"(regression: it cleared only GameObjects)\n");
	SceneManagement::Scene* scene = MakeScene();
	GameMap& map = scene->gameMap;

	Entity entity{};
	map.SpawnEntity(entity);
	InteractableObject interactable(INTERACT_ITEM, 0);
	map.SpawnInteractable(interactable);
	GameObject block{};
	map.SpawnGameObject(block);

	const Vector3 size = Vector3{ 12.0f, 1.0f, 8.0f };
	map.create(size);

	Check(map.EntityCount() == 0 && map.InteractableCount() == 0, "the entities and interactables are gone");
	Check(map.GameObjectCount() == 1, "one object is left");

	bool floorMatches = false;
	map.ForEachGameObject([&](GameObject& floor) { floorMatches = floor.rigidBody3D.scale == size; });
	Check(floorMatches, "and it is a floor the size of the world");

	delete scene;
}

// ─── ForEach ───────────────────────────────────────────────────────────────

static void TestForEachGameObjectVisitsAllAndHonoursEarlyExit()
{
	std::printf("ForEachGameObject visits every live object and stops early on a false return\n");
	SceneManagement::Scene* scene = MakeScene();
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

int main()
{
	// Spawn and Restore bind a model, so a GL context has to exist before any
	// object enters a map — see StalkerFsmTests.cpp.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-gamemap-tests");

	std::printf("GameMap tests\n\n");

	TestIdsUniqueAcrossKinds();
	TestFindPointerSurvivesFurtherSpawns();
	TestSpawnEntityKeysByNewIdAndDoesNotClobberSiblings();
	TestANewObjectHoldsNoModel();
	TestSpawnLeavesThePrototypesModelAlone();
	TestSpawnGivesEveryKindAUnitScale();
	TestRestoreKeepsWhatWasSaved();
	TestRestoreRaisesTheNextId();
	TestRestoreRefusesAnIdInUse();
	TestDestroyIsDeferredForEveryKind();
	TestDestroyInsideForEachIsSafe();
	TestUnlockRemovesTheStationAtTheSweep();
	TestAPickedUpItemIsCarriedById();
	TestClearEmptiesEveryKindAndRestartsIds();
	TestClearWorldForgetsWhatThePlayerCarries();
	TestCreateReplacesTheWholeWorld();
	TestForEachGameObjectVisitsAllAndHonoursEarlyExit();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
