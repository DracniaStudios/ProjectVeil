/**
 * Behavioural tests for SaveSystem: what a Game save and a World save keep,
 * what loading one replaces, and which saves a load turns away untouched.
 *
 * Every round trip runs twice: once through the game's own SaveGame/LoadGame
 * and SaveWorld/LoadWorld, on real files under resources/scenes/ and saves/
 * (named TEST_SAVE and removed again afterwards), and once through a
 * MemorySaveStore, which keeps each save as the text a file would hold.
 *
 * Same tier as GameMapTests.cpp: loading Restores objects, which binds a model,
 * which needs a GL context, so a window is opened and this links against the
 * game's own object files. See tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <SaveSystem.h>
#include <LightingSystem.h>
#include <gameMap.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
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

namespace fs = std::filesystem;
using Json = nlohmann::json;

static const char* TEST_SAVE = "__save_system_tests";

static fs::path WorldFile() { return fs::path(RESOURCES_PATH "scenes/") / (std::string(TEST_SAVE) + ".json"); }
static fs::path GameFile() { return fs::path(RESOURCES_PATH "../saves/") / (std::string(TEST_SAVE) + ".json"); }

// Removes the test save and the backup and temp files a save leaves beside it
static void RemoveTestFiles()
{
	std::error_code ignored;
	for (const fs::path& file : { WorldFile(), GameFile() })
	{
		fs::remove(file, ignored);
		fs::remove(file.string() + ".bak", ignored);
		fs::remove(file.string() + ".tmp", ignored);
	}
}

static void WriteFile(const fs::path& path, const std::string& text)
{
	std::ofstream file(path, std::ios::binary);
	file << text;
}

// Scene_new() so scene->player exists and the scene is current
static SceneManagement::Scene* MakeScene()
{
	return SceneManagement::Scene_new();
}

static GameObject* FindByName(GameMap& map, const std::string& name)
{
	GameObject* found = nullptr;
	map.ForEachObject([&](GameObject& object) { if (object.name == name) { found = &object; } });
	return found;
}

static int CountOfType(GameMap& map, ObjectType type)
{
	int count = 0;
	map.ForEachObject([&](GameObject& object) { if (object.type == type) { ++count; } });
	return count;
}

// ─── Where saves go ────────────────────────────────────────────────────────

// One way of saving and loading both scopes. Every round-trip test below runs
// once per store, so a store that drops something fails the same checks.
struct Store
{
	const char* label;
	std::function<bool(SceneManagement::Scene&)> saveWorld;
	std::function<bool(SceneManagement::Scene&)> loadWorld;
	std::function<bool(SceneManagement::Scene&)> saveGame;
	std::function<bool(SceneManagement::Scene&)> loadGame;
};

static Store FileStore()
{
	return Store{
		"files",
		[](SceneManagement::Scene& scene) { return SaveSystem::SaveWorld(TEST_SAVE, &scene); },
		[](SceneManagement::Scene& scene) { return SaveSystem::LoadWorld(TEST_SAVE, scene); },
		[](SceneManagement::Scene& scene) { return SaveSystem::SaveGame(TEST_SAVE, &scene); },
		[](SceneManagement::Scene& scene) { return SaveSystem::LoadGame(TEST_SAVE, scene); },
	};
}

// The same two scopes through Save/Load on an in-memory store, lit by the
// LightingSystem the way the game's own saves are
static Store MemoryStore(SaveSystem::MemorySaveStore& memory)
{
	using SaveSystem::SaveScope;
	return Store{
		"memory",
		[&memory](SceneManagement::Scene& scene) { return SaveSystem::Save(memory, "world", scene, SaveScope::World, { SaveSystem::LightingParticipant() }); },
		[&memory](SceneManagement::Scene& scene) { return SaveSystem::Load(memory, "world", scene, SaveScope::World, { SaveSystem::LightingParticipant() }); },
		[&memory](SceneManagement::Scene& scene) { return SaveSystem::Save(memory, "game", scene, SaveScope::Game, { SaveSystem::LightingParticipant() }); },
		[&memory](SceneManagement::Scene& scene) { return SaveSystem::Load(memory, "game", scene, SaveScope::Game, { SaveSystem::LightingParticipant() }); },
	};
}

// A world with one of each kind, every one of them changed from how it spawned
struct BuiltWorld
{
	std::uint64_t doorId = 0;
	std::uint64_t stalkerId = 0;
	std::uint64_t stationId = 0;
	std::uint64_t nextId = 0;
};

static BuiltWorld BuildWorld(SceneManagement::Scene& scene)
{
	BuiltWorld built;
	GameMap& map = scene.gameMap;
	map.size = Vector3{ 40, 12, 30 };

	GameObject doorPrototype{};
	doorPrototype.name = "Door";
	GameObject* door = map.SpawnGameObject(doorPrototype);
	door->rigidBody3D.Teleport(Vector3{ 3, 1, -4 });
	door->isEnabled = false;
	built.doorId = door->id;

	auto stalkerPrototype = Entity::createByKind(ENTITYKIND_STALKER);
	stalkerPrototype->name = "Stalker";
	Entity* stalker = map.SpawnEntity(*stalkerPrototype);
	stalker->maxHealth = 10.0f;
	stalker->health = 3.0f;
	built.stalkerId = stalker->id;

	InteractableObject stationPrototype(INTERACT_MINIGAME, 2, 0);
	stationPrototype.name = "Station";
	InteractableObject* station = map.SpawnInteractable(stationPrototype);
	station->targetId = built.doorId;
	station->isCompleted = true;
	built.stationId = station->id;

	built.nextId = map.NextId();
	return built;
}

// ─── Round trips ───────────────────────────────────────────────────────────

static void TestWorldRoundTripKeepsEveryKind(const Store& store)
{
	std::printf("[%s] a World save Restores every kind under its own id, as it was saved\n", store.label);
	SceneManagement::Scene* saved = MakeScene();
	const BuiltWorld built = BuildWorld(*saved);
	Check(store.saveWorld(*saved), "the world saves");

	SceneManagement::Scene* loaded = MakeScene();
	Check(store.loadWorld(*loaded), "and loads into another scene");
	GameMap& map = loaded->gameMap;

	Check(map.size.x == 40 && map.size.y == 12 && map.size.z == 30, "the map keeps its size");
	Check(map.GameObjectCount() == 1 && map.EntityCount() == 1 && map.InteractableCount() == 1,
		"one object of each kind comes back, and nothing else");

	GameObject* door = map.FindGameObject(built.doorId);
	Check(door != nullptr && door->name == "Door", "the door keeps its id");
	Check(door != nullptr && !door->isEnabled, "a door saved disabled stays disabled");
	Check(door != nullptr && door->rigidBody3D.translation.x == 3 && door->rigidBody3D.translation.z == -4,
		"and stays where it was");

	Entity* stalker = map.FindEntity(built.stalkerId);
	Check(stalker != nullptr && stalker->kind == ENTITYKIND_STALKER, "the Stalker comes back a Stalker");
	Check(stalker != nullptr && stalker->health == 3.0f, "and stays hurt");

	InteractableObject* station = map.FindInteractable(built.stationId);
	Check(station != nullptr && station->targetId == built.doorId, "the Task station keeps its Target");
	Check(station != nullptr && station->isCompleted, "and stays completed");
	Check(station != nullptr && station->variation == 2, "and keeps its MiniGame");

	Check(map.NextId() == built.nextId, "the next Spawn gets the id it would have got before the save");

	delete saved;
	delete loaded;
}

static void TestWorldSaveLeavesOutTheMomentAndThePlayer(const Store& store)
{
	std::printf("[%s] a World save holds the level only: no projectiles in flight, "
		"and loading one leaves the player and scene flags alone\n", store.label);
	SceneManagement::Scene* saved = MakeScene();
	BuildWorld(*saved);
	GameObject projectile{};
	projectile.type = OBJECT_PROJECTILE;
	saved->gameMap.SpawnGameObject(projectile);
	saved->limitYBounds = true;
	saved->player->health = 1.0f;
	Check(store.saveWorld(*saved), "the world saves");

	SceneManagement::Scene* loaded = MakeScene();
	loaded->player->rigidBody3D.Teleport(Vector3{ 7, 2, 7 });
	const float playerHealth = loaded->player->health;
	Check(store.loadWorld(*loaded), "and loads");

	Check(CountOfType(loaded->gameMap, OBJECT_PROJECTILE) == 0, "the projectile was not saved");
	Check(loaded->player->rigidBody3D.translation.x == 7 && loaded->player->rigidBody3D.translation.z == 7,
		"the player stays where they were");
	Check(loaded->player->health == playerHealth, "and keeps their own health");
	Check(!loaded->limitYBounds, "the scene's flags are left alone");

	delete saved;
	delete loaded;
}

static void TestGameRoundTripKeepsTheMomentAndThePlayer(const Store& store)
{
	std::printf("[%s] a Game save holds the moment the player left: the world, what is in "
		"flight, the scene's flags and the player\n", store.label);
	SceneManagement::Scene* saved = MakeScene();
	const BuiltWorld built = BuildWorld(*saved);
	GameObject projectile{};
	projectile.type = OBJECT_PROJECTILE;
	saved->gameMap.SpawnGameObject(projectile);
	saved->limitYBounds = true;
	saved->player->rigidBody3D.Teleport(Vector3{ -6, 1, 9 });
	saved->player->health = 2.0f;
	Check(store.saveGame(*saved), "the game saves");

	SceneManagement::Scene* loaded = MakeScene();
	Check(store.loadGame(*loaded), "and loads into another scene");

	Check(loaded->gameMap.FindGameObject(built.doorId) != nullptr
		&& loaded->gameMap.FindEntity(built.stalkerId) != nullptr
		&& loaded->gameMap.FindInteractable(built.stationId) != nullptr,
		"every kind comes back under its own id");
	Check(CountOfType(loaded->gameMap, OBJECT_PROJECTILE) == 1, "the projectile in flight comes back");
	Check(loaded->limitYBounds, "the scene's flags come back");
	Check(loaded->player->rigidBody3D.translation.x == -6 && loaded->player->rigidBody3D.translation.z == 9,
		"the player is where they were");
	Check(loaded->player->health == 2.0f, "and as hurt as they were");
	Check(loaded->player->id == PLAYER_ID, "and is still the player");
	Check(loaded->gameMap.FindEntity(PLAYER_ID) == nullptr, "and is not also a world entity");

	delete saved;
	delete loaded;
}

static void TestLoadReplacesTheWorld(const Store& store)
{
	std::printf("[%s] loading replaces everything already in the world, and empties what "
		"the player carries\n", store.label);
	SceneManagement::Scene* saved = MakeScene();
	BuildWorld(*saved);
	Check(store.saveWorld(*saved), "the world saves");
	Check(store.saveGame(*saved), "the game saves");

	for (int scope = 0; scope < 2; ++scope)
	{
		const char* which = scope == 0 ? "World" : "Game";
		SceneManagement::Scene* loaded = MakeScene();
		GameObject leftoverPrototype{};
		leftoverPrototype.name = "Leftover";
		GameObject* leftover = loaded->gameMap.SpawnGameObject(leftoverPrototype);
		loaded->player->inventory.push_back(leftover->id);

		Check(scope == 0 ? store.loadWorld(*loaded) : store.loadGame(*loaded),
			std::string(which) + " save loads");
		Check(FindByName(loaded->gameMap, "Leftover") == nullptr,
			std::string("the object already in the world is gone after a ") + which + " load");
		Check(loaded->player->inventory.empty(),
			std::string("and the player carries nothing after a ") + which + " load");

		delete loaded;
	}

	delete saved;
}

static void TestLightingTravelsWithTheWorld(const Store& store)
{
	std::printf("[%s] the level's lights are saved with it and come back on load\n", store.label);
	Lighting::LightingSystem& lighting = Lighting::LightingSystem::getInstance();

	SceneManagement::Scene* saved = MakeScene();
	lighting.ClearLights();
	Lighting::Light lantern{};
	lantern.name = "Lantern";
	lighting.AddLight(lantern);
	Check(store.saveWorld(*saved), "the world saves");

	lighting.CreateDefaultRig();
	SceneManagement::Scene* loaded = MakeScene();
	Check(store.loadWorld(*loaded), "and loads");
	Check(lighting.GetLights().size() == 1 && lighting.GetLights()[0].name == "Lantern",
		"the saved lights replace the ones that were up");

	delete saved;
	delete loaded;
}

// ─── Saves a load turns away ───────────────────────────────────────────────

// A scene with one object in it, to see whether a refused load touched it
static SceneManagement::Scene* MakeOccupiedScene()
{
	SceneManagement::Scene* scene = MakeScene();
	GameObject keeperPrototype{};
	keeperPrototype.name = "Keeper";
	GameObject* keeper = scene->gameMap.SpawnGameObject(keeperPrototype);
	scene->player->inventory.push_back(keeper->id);
	return scene;
}

static void CheckUntouched(SceneManagement::Scene& scene, const std::string& after)
{
	Check(FindByName(scene.gameMap, "Keeper") != nullptr && scene.gameMap.GameObjectCount() == 1,
		"the world is untouched after " + after);
	Check(scene.player->inventory.size() == 1, "and so is what the player carries after " + after);
}

static void TestANewerSaveLeavesTheWorldUntouched()
{
	std::printf("a save written by a newer game is turned away before the world is cleared\n");
	const std::string newer = R"({ "Version": 99, "Objects": {}, "Entities": {}, "Interactables": {} })";
	WriteFile(WorldFile(), newer);
	WriteFile(GameFile(), newer);

	SceneManagement::Scene* scene = MakeOccupiedScene();
	Check(!SaveSystem::LoadWorld(TEST_SAVE, *scene), "a newer World save is refused");
	CheckUntouched(*scene, "a refused World load");
	Check(!SaveSystem::LoadGame(TEST_SAVE, *scene), "a newer Game save is refused");
	CheckUntouched(*scene, "a refused Game load");

	delete scene;
	RemoveTestFiles();
}

static void TestACorruptOrMissingSaveLeavesTheWorldUntouched()
{
	std::printf("a save that is missing or is not JSON is turned away before the world is cleared\n");
	RemoveTestFiles();

	SceneManagement::Scene* scene = MakeOccupiedScene();
	Check(!SaveSystem::LoadWorld(TEST_SAVE, *scene), "a missing World save is refused");
	Check(!SaveSystem::LoadGame(TEST_SAVE, *scene), "a missing Game save is refused");
	CheckUntouched(*scene, "a missing save");

	WriteFile(WorldFile(), "{ not json");
	WriteFile(GameFile(), "{ not json");
	Check(!SaveSystem::LoadWorld(TEST_SAVE, *scene), "a corrupt World save is refused");
	Check(!SaveSystem::LoadGame(TEST_SAVE, *scene), "a corrupt Game save is refused");
	CheckUntouched(*scene, "a corrupt save");

	delete scene;
	RemoveTestFiles();
}

static void TestAWorldWithoutLightingGetsTheDefaultRig()
{
	std::printf("a World save from before lighting existed loads lit, with the default rig\n");
	Lighting::LightingSystem& lighting = Lighting::LightingSystem::getInstance();
	lighting.ClearLights();

	WriteFile(WorldFile(), R"({ "Version": 2, "WorldOnly": true, "IdCounter": 5,
		"Map": { "SizeX": 10, "SizeY": 10, "SizeZ": 10 },
		"Objects": {}, "Entities": {}, "Interactables": {} })");

	SceneManagement::Scene* scene = MakeScene();
	Check(SaveSystem::LoadWorld(TEST_SAVE, *scene), "a version 2 World save still loads");
	Check(!lighting.GetLights().empty() && lighting.GetLights()[0].name == "Moonlight",
		"and the world is lit by the default rig");
	Check(scene->gameMap.NextId() >= 5, "and its ids stay reserved");

	delete scene;
	RemoveTestFiles();
}

static void TestAMalformedSaveLeavesTheWorldUntouched()
{
	std::printf("a save with a section of the wrong shape is turned away before the world is "
		"cleared (regression: Apply read them with value(), which throws, after the clear)\n");
	const std::vector<std::pair<const char*, const char*>> malformed = {
		{ "a save that is not an object", R"([1, 2, 3])" },
		{ "a Version that is not a number", R"({ "Version": "3" })" },
		{ "an IdCounter that is not a number", R"({ "Version": 3, "IdCounter": "many" })" },
		{ "a Map that is not an object", R"({ "Version": 3, "Map": 5 })" },
		{ "a Map size that is not a number", R"({ "Version": 3, "Map": { "SizeX": "wide" } })" },
		{ "an Objects section that is a list", R"({ "Version": 3, "Objects": [ {} ] })" },
		{ "an Entities section that is a string", R"({ "Version": 3, "Entities": "none" })" },
		{ "an entity that is not an object", R"({ "Version": 3, "Entities": { "5": 3 } })" },
		{ "a LimitYBounds that is not true or false", R"({ "Version": 3, "Scene": { "LimitYBounds": "yes" } })" },
		{ "a Player that is not an object", R"({ "Version": 3, "Player": null })" },
	};

	for (const auto& [what, text] : malformed)
	{
		SaveSystem::MemorySaveStore memory;
		memory.Write("bad", Json::parse(text));

		bool participantRan = false;
		const SaveSystem::SaveParticipant watcher{ "Watcher", [] { return Json::object(); },
			[&participantRan](const Json*) { participantRan = true; } };

		SceneManagement::Scene* scene = MakeOccupiedScene();
		scene->limitYBounds = true;
		Check(!SaveSystem::Load(memory, "bad", *scene, SaveSystem::SaveScope::Game, { watcher }),
			std::string(what) + " is refused");
		CheckUntouched(*scene, std::string("refusing ") + what);
		Check(scene->limitYBounds, std::string("the scene's flags are untouched after refusing ") + what);
		Check(!participantRan, std::string("no participant is applied after refusing ") + what);
		delete scene;
	}
}

static void TestAParticipantKeepsItsOwnSection()
{
	std::printf("a participant's section is written under its key, handed back on load, and "
		"missing from a save that never had it\n");
	Json weather = { { "Rain", 0.5 } };
	std::vector<std::string> applied;
	const SaveSystem::SaveParticipant participant{
		"Weather",
		[&weather] { return weather; },
		[&applied](const Json* section) { applied.push_back(section != nullptr ? section->dump() : "none"); },
	};

	SaveSystem::MemorySaveStore memory;
	SceneManagement::Scene* scene = MakeScene();
	Check(SaveSystem::Save(memory, "world", *scene, SaveSystem::SaveScope::World, { participant }), "the world saves");

	Json written;
	Check(memory.Read("world", written) && written.contains("Weather") && written["Weather"]["Rain"] == 0.5,
		"the participant's section is in the save under its key");
	Check(!written.contains("Lighting"), "and a save without the Lighting participant holds no lighting");

	weather["Rain"] = 0.0;
	Check(SaveSystem::Load(memory, "world", *scene, SaveSystem::SaveScope::World, { participant }), "and loads");
	Check(applied.size() == 1 && Json::parse(applied[0])["Rain"] == 0.5, "the participant gets its saved section back");

	memory.Write("bare", Json{ { "Version", 3 } });
	Check(SaveSystem::Load(memory, "bare", *scene, SaveSystem::SaveScope::World, { participant }), "a save without the section loads");
	Check(applied.size() == 2 && applied[1] == "none", "and the participant is told it has none");

	delete scene;
}

static void TestTheScopeDecidesWhatASaveHolds()
{
	std::printf("the scope, not a second copy of the pipeline, decides which sections a save holds\n");
	SaveSystem::MemorySaveStore memory;
	SceneManagement::Scene* scene = MakeScene();
	BuildWorld(*scene);
	Check(SaveSystem::Save(memory, "world", *scene, SaveSystem::SaveScope::World, { SaveSystem::LightingParticipant() }), "the world saves");
	Check(SaveSystem::Save(memory, "game", *scene, SaveSystem::SaveScope::Game, { SaveSystem::LightingParticipant() }), "the game saves");

	Json world;
	Json game;
	memory.Read("world", world);
	memory.Read("game", game);

	for (const char* section : { "Version", "IdCounter", "Map", "Lighting", "Objects", "Entities", "Interactables" })
	{
		Check(world.contains(section) && game.contains(section), std::string("both saves hold ") + section);
	}
	Check(world.value("WorldOnly", false) && !game.contains("WorldOnly"), "only the World save is marked WorldOnly");
	Check(!world.contains("Player") && game.contains("Player"), "only the Game save holds the player");
	Check(!world.contains("Scene") && game.contains("Scene"), "only the Game save holds the scene's flags");

	delete scene;
}

static void TestTheShippedWorldsStillLoad()
{
	std::printf("the World saves the game ships load every object they hold\n");
	const SaveSystem::FileSaveStore worlds(RESOURCES_PATH "scenes/");

	for (const char* name : { "mainMenu", "chunk_1" })
	{
		Json file;
		if (!worlds.Read(name, file))
		{
			Check(false, std::string(name) + " can be read");
			continue;
		}

		// No participants: the LightingSystem is never initialised here, and these
		// checks are about the objects
		SceneManagement::Scene* scene = MakeScene();
		Check(SaveSystem::Load(worlds, name, *scene, SaveSystem::SaveScope::World, {}), std::string(name) + " loads");
		Check(scene->gameMap.GameObjectCount() == file["Objects"].size()
			&& scene->gameMap.EntityCount() == file["Entities"].size()
			&& scene->gameMap.InteractableCount() == file["Interactables"].size(),
			std::string(name) + " Restores every object in the file");
		Check(scene->gameMap.NextId() >= file.value("IdCounter", std::uint64_t{ 0 }),
			std::string(name) + " keeps its ids reserved");
		delete scene;
	}
}

int main()
{
	// Loading Restores objects, which binds a model, so a GL context has to exist
	// before any object enters a map — see StalkerFsmTests.cpp.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-save-system-tests");

	std::printf("SaveSystem tests\n\n");
	RemoveTestFiles();

	SaveSystem::MemorySaveStore memory;
	for (const Store& store : { FileStore(), MemoryStore(memory) })
	{
		TestWorldRoundTripKeepsEveryKind(store);
		TestWorldSaveLeavesOutTheMomentAndThePlayer(store);
		TestGameRoundTripKeepsTheMomentAndThePlayer(store);
		TestLoadReplacesTheWorld(store);
		TestLightingTravelsWithTheWorld(store);
		RemoveTestFiles();
	}

	TestANewerSaveLeavesTheWorldUntouched();
	TestACorruptOrMissingSaveLeavesTheWorldUntouched();
	TestAWorldWithoutLightingGetsTheDefaultRig();
	TestAMalformedSaveLeavesTheWorldUntouched();
	TestAParticipantKeepsItsOwnSection();
	TestTheScopeDecidesWhatASaveHolds();
	TestTheShippedWorldsStillLoad();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	RemoveTestFiles();
	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
