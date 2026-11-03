/**
 * The World store and the world key (MultiplayerSplitPlan.md W4; rules W-6,
 * W-8, W-9, W-11; decisions Q37, Q47, Q60).
 *
 *   - FileWorldStore opens at once, keeps the session's World state in memory
 *     and writes nothing.
 *   - world_id and world keys follow the module's rule.
 *   - The content hash is the same for every load of the same World save, is
 *     blind to runtime fields, and changes for any static edit, so an edited
 *     World save opens a fresh world.
 */

#include <raylib.h>

#include <Scene.h>
#include <SaveSystem.h>
#include <AI/Stalker.h>
#include <World/FileWorldStore.h>
#include <World/WorldKey.h>

#include <cstdio>
#include <filesystem>
#include <string>

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

static void TestFileWorldStore()
{
	std::printf("FileWorldStore: session memory only\n");

	World::FileWorldStore store;
	World::WorldState state;
	Check(!store.ReadState(state), "nothing can be read before BeginOpen (W-6)");

	const auto savesBefore = std::distance(std::filesystem::directory_iterator(RESOURCES_PATH "../saves"), std::filesystem::directory_iterator{});

	store.BeginOpen("chunk_1:0123456789abcdef");
	Check(store.Status() == World::WorldStoreStatus::Ready, "opening is immediate");
	Check(store.ReadState(state) && state.completedStations.empty(), "a fresh session starts with no World state");

	store.SubmitStationCompleted(302);
	store.SubmitStationCompleted(302);
	store.SubmitStationCompleted(312);
	Check(store.ReadState(state) && state.completedStations.size() == 2, "completions are kept once each");
	Check(store.Pending() == 0, "nothing is ever pending");

	store.SubmitFullWipe();
	Check(store.ReadState(state) && state.completedStations.empty(), "a Full wipe clears them");

	store.BeginOpen("other:0000000000000000");
	Check(store.Key() == "chunk_1:0123456789abcdef", "BeginOpen runs once per store");

	store.Close(std::chrono::milliseconds(0));
	store.Close(std::chrono::milliseconds(0));
	Check(store.Status() == World::WorldStoreStatus::Closed, "Close is idempotent");
	store.SubmitStationCompleted(1);
	Check(!store.ReadState(state), "after Close, Submit is ignored and nothing is read");

	const auto savesAfter = std::distance(std::filesystem::directory_iterator(RESOURCES_PATH "../saves"), std::filesystem::directory_iterator{});
	Check(savesBefore == savesAfter, "no new file appears in saves/");
}

static void TestWorldIdsAndKeys()
{
	std::printf("world ids and keys (W-8)\n");

	Check(World::IsValidWorldId("chunk_1") && World::IsValidWorldId("a") && World::IsValidWorldId(std::string(32, 'z')), "valid ids");
	Check(!World::IsValidWorldId("") && !World::IsValidWorldId(std::string(33, 'a')), "empty and over-long ids are refused");
	Check(!World::IsValidWorldId("Chunk_1") && !World::IsValidWorldId("a b") && !World::IsValidWorldId("a'; DROP") && !World::IsValidWorldId("a:b"), "upper case, spaces, quotes and colons are refused");

	Check(World::WorldIdFromName("Chunk 1!") == "chunk_1_", "a World save name becomes a valid id");
	Check(World::WorldIdFromName("") == "world", "an empty name becomes \"world\"");
	Check(World::IsValidWorldId(World::WorldIdFromName(std::string(50, 'Q'))), "a long name is cut to 32");

	const std::string key = World::MakeWorldKey("chunk_1", 0xabcULL);
	Check(key == "chunk_1:0000000000000abc", "the key is id, colon, 16 lower-case hex digits");
	Check(World::IsValidWorldKey(key), "and is a valid key");
	Check(!World::IsValidWorldKey("chunk_1:0000000000000ABC") && !World::IsValidWorldKey("chunk_1:abc") && !World::IsValidWorldKey("Chunk:0000000000000abc"), "malformed keys are refused");
}

static SceneManagement::Scene* LoadChunk()
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	scene->SetSpawnPoint(Vector3(20, 2, 0));
	SaveSystem::LoadWorld("chunk_1", *scene);
	return scene;
}

static void TestContentHash()
{
	std::printf("the content hash (W-9)\n");

	SceneManagement::Scene* a = LoadChunk();
	SceneManagement::Scene* b = LoadChunk();
	const std::uint64_t hash = World::ContentHash(*a);
	std::printf("  chunk_1 content hash: %016llx (record it in the test log; the laptop must print the same)\n", static_cast<unsigned long long>(hash));
	Check(hash == World::ContentHash(*b), "two loads of the same World save hash the same");

	// Runtime fields are not static data
	if (InteractableObject* station = a->gameMap.FindInteractable(312)) { station->isCompleted = true; station->isRunningMiniGame = true; }
	a->gameMap.ForEachEntity([](Entity& e) {
		if (e.kind != ENTITYKIND_STALKER) { return; }
		Stalker& s = static_cast<Stalker&>(e);
		s.state = STALKER_HUNT;
		s.rigidBody3D.SetVelocity(Vector3(3, 0, 1));
		s.rigidBody3D.Teleport(Vector3(1, 2, 3)); // the stalker is not static: where it stands is runtime
	});
	a->ForEachPlayer([](int, Player& p) { p.rigidBody3D.Teleport(Vector3(9, 9, 9)); });
	Check(World::ContentHash(*a) == hash, "runtime fields (completion, stalker state and place, players) don't change it");

	// Static edits do, one at a time
	SceneManagement::Scene* c = LoadChunk();
	bool movedWall = false;
	c->gameMap.ForEachGameObject([&](GameObject& o) {
		if (!movedWall && o.rigidBody3D.isStatic && o.rigidBody3D.collider.canCollide) { o.rigidBody3D.translation.x += 0.01f; movedWall = true; }
	});
	Check(movedWall && World::ContentHash(*c) != hash, "moving a wall changes it");
	SceneManagement::Scene_delete(c);

	c = LoadChunk();
	if (InteractableObject* station = c->gameMap.FindInteractable(312)) { station->minimumSeconds += 1.0f; }
	Check(World::ContentHash(*c) != hash, "a station's minimum seconds change it");
	SceneManagement::Scene_delete(c);

	c = LoadChunk();
	if (InteractableObject* station = c->gameMap.FindInteractable(312)) { station->targetId = 301; }
	Check(World::ContentHash(*c) != hash, "a station's Target changes it");
	SceneManagement::Scene_delete(c);

	c = LoadChunk();
	c->gameMap.ForEachEntity([](Entity& e) { if (e.kind == ENTITYKIND_STALKER) { static_cast<Stalker&>(e).waypoints.push_back(Vector3(0, 0, 0)); } });
	Check(World::ContentHash(*c) != hash, "a patrol waypoint changes it");
	SceneManagement::Scene_delete(c);

	c = LoadChunk();
	c->SetSpawnPoint(Vector3(21, 2, 0));
	Check(World::ContentHash(*c) != hash, "the spawn point changes it");
	SceneManagement::Scene_delete(c);

	// -0 and +0 are the same number
	World::Fnv1a64 plus, minus;
	plus.Float(0.0f);
	minus.Float(-0.0f);
	Check(plus.Value() == minus.Value(), "-0 hashes as +0");

	// An edited World save opens a fresh world
	Check(World::MakeWorldKey("chunk_1", hash) != World::MakeWorldKey("chunk_1", hash ^ 1), "a different hash is a different world key");

	SceneManagement::Scene_delete(a);
	SceneManagement::Scene_delete(b);
}

int main()
{
	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(64, 64, "veil-world-store-tests");

	std::printf("World store tests\n\n");

	TestFileWorldStore();
	TestWorldIdsAndKeys();
	TestContentHash();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
