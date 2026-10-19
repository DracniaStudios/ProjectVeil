/**
 * Tests for Task station state: what winning, quitting and Practice runs do to
 * a station and its Target, through the Scene's MiniGame methods.
 *
 * A station had no way to say it had ever been finished until it carried its
 * own completion flag. The Director reads that flag to tell an outstanding
 * objective from a done one, so a regression here is a regression in what the
 * AI applies pressure toward — not just bookkeeping.
 *
 * The session's own rules (retrying a lost round, the two-deep progress
 * history) are tested without a window in MiniGameSessionTests.
 *
 * Like the other engine-linked suites this needs a GL context for GameObject's
 * fallback cube. See tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <AI/Director.h>
#include <AI/Stalker.h>

#include <cstdio>
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

static SceneManagement::Scene* MakeScene()
{
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	SceneManagement::SceneManager::getInstance().currentScene = scene;
	scene->player = new Player();
	scene->player->id = PLAYER_ID;
	// Everything unlocked, so ActivateMiniGame never bails at the unlock gate
	// and these cases are about station state rather than progression.
	scene->player->artifactUnlocked = MINI_GAME_RO_SHAM_BOO_ID;
	return scene;
}

static InteractableObject* AddStation(SceneManagement::Scene* scene, Vector3 at, int miniGameId,
                                      std::uint64_t id)
{
	auto owned = std::make_unique<InteractableObject>(INTERACT_MINIGAME, miniGameId, 0);
	owned->id = id;
	owned->name = "TestStation";
	owned->isEnabled = true;
	owned->isInteractable = true;
	owned->rigidBody3D.Teleport(at);
	InteractableObject* station = owned.get();
	scene->gameMap.LoadInteractable(std::move(owned));
	return station;
}

// ─── Completion ────────────────────────────────────────────────────────────

static void TestStationStartsIncomplete()
{
	std::printf("a fresh station is not complete\n");
	SceneManagement::Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 900);

	Check(!station->isCompleted, "a placed station starts outstanding");
	delete scene;
}

static void TestWinningCompletesTheStation()
{
	std::printf("winning a station's minigame completes the station\n");
	SceneManagement::Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 901);
	station->onInteract();

	Check(scene->GetRunningStation() == station, "the station is occupied while it runs");
	Check(scene->overlays.Top() == Overlay::MiniGame, "the MiniGame overlay is up");

	scene->WinMiniGame();

	Check(station->isCompleted, "winning completes the station");
	Check(!station->isInteractable, "a completed station can't be started again");
	// isRunningMiniGame is "occupied right now" and clears when the game ends;
	// isCompleted is "this work is done" and must not.
	Check(!station->isRunningMiniGame, "the station is free again");
	Check(!scene->overlays.IsOpen(Overlay::MiniGame), "the MiniGame overlay is closed");
	Check(!station->isEnabled, "with no Target, the station toggles itself");
	delete scene;
}

static void TestQuittingLeavesTheStationOutstanding()
{
	std::printf("walking away leaves the station outstanding\n");
	SceneManagement::Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 902);
	station->onInteract();

	scene->QuitMiniGame();

	Check(!station->isCompleted, "quitting does not complete the station");
	Check(!station->isRunningMiniGame, "but it does free it");
	Check(!scene->overlays.IsOpen(Overlay::MiniGame), "and closes the MiniGame overlay");
	Check(station->isEnabled, "nothing was toggled");
	delete scene;
}

static GameObject* AddDoor(SceneManagement::Scene* scene, std::uint64_t id)
{
	GameObject door;
	door.id = id;
	door.name = "TestDoor";
	return scene->gameMap.LoadGameObject(door);
}

static void TestWinningTogglesTheTarget()
{
	std::printf("winning toggles the station's Target, not the station\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* door = AddDoor(scene, 950);
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 903);
	station->targetId = door->id;

	Check(station->FindMiniGameTarget(scene->gameMap) == door, "the station names its Target");
	Check(door->isEnabled, "the door starts enabled");

	station->onInteract();
	scene->WinMiniGame();

	door = scene->gameMap.FindGameObject(950);
	Check(door != nullptr && !door->isEnabled, "the Target is toggled");
	Check(station->isEnabled, "the station itself is left alone");
	Check(station->isCompleted, "and the station is complete");
	delete scene;
}

static void TestMissingTargetStillCompletes()
{
	std::printf("a station whose Target is gone still completes\n");
	SceneManagement::Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 904);
	station->targetId = 9999; // Nothing has this id

	Check(station->FindMiniGameTarget(scene->gameMap) == nullptr, "a missing Target is reported, not guessed");

	station->onInteract();
	scene->WinMiniGame();

	// A deleted door must not un-complete finished work, or the Director would
	// keep hinting at it forever
	Check(station->isCompleted, "the station is complete anyway");
	delete scene;
}

static void TestPracticeRunCompletesNothing()
{
	std::printf("a Practice run grants the buff but completes and toggles nothing\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* door = AddDoor(scene, 951);
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 905);
	station->targetId = door->id;

	// Started from the Artifact: no station
	Check(scene->StartMiniGame(MINI_GAME_CRANE_ID), "the practice game starts");
	Check(scene->GetRunningStation() == nullptr, "no station is occupied");
	scene->WinMiniGame();

	CooldownTimer* range = scene->player->getBuff(BUFF_RANGE);
	Check(range != nullptr && range->remaining_time() > 0, "winning Crane grants the Range buff");
	Check(!station->isCompleted, "no station is completed");
	door = scene->gameMap.FindGameObject(951);
	Check(door != nullptr && door->isEnabled, "and no door is toggled");
	delete scene;
}

static void TestUnknownGameStartsNothing()
{
	std::printf("an id that names no game starts nothing\n");
	SceneManagement::Scene* scene = MakeScene();

	Check(!scene->StartMiniGame(999), "the start is refused");
	Check(!scene->miniGameSession.IsRunning(), "no game runs");
	Check(!scene->overlays.IsOpen(Overlay::MiniGame), "and no overlay opens");
	delete scene;
}

static void TestCompletionRoundTripsThroughJson()
{
	std::printf("completion survives a save/load round trip\n");
	SceneManagement::Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 2, 0, 3 }, MINI_GAME_CRANE_ID, 906);
	station->isCompleted = true;
	station->isRunningMiniGame = true;
	station->targetId = 77;

	Json saved = station->formatToJson();

	InteractableObject loaded;
	Check(loaded.loadFromJson(saved), "the station loads back");
	Check(loaded.isCompleted, "completion is persisted");
	Check(loaded.targetId == 77, "the Target is persisted");
	// Occupancy is a live fact about someone standing there, and a reloaded save
	// has nobody at the station.
	Check(!loaded.isRunningMiniGame, "occupancy is not persisted");
	delete scene;
}

static void TestOldSavesKeepTheirTarget()
{
	std::printf("a station saved before Targets existed keeps its Target\n");
	SceneManagement::Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 907);

	// Written the old way: the Target in Activator, and no TargetId
	Json saved = station->formatToJson();
	saved.erase("TargetId");
	saved["Activator"] = 55;

	InteractableObject loaded;
	Check(loaded.loadFromJson(saved), "the old station loads");
	Check(loaded.targetId == 55, "its Activator becomes its Target");
	Check(loaded.activator == 0, "and is no longer an activator value");
	delete scene;
}

// ─── Director reads completion ─────────────────────────────────────────────

static void TestDirectorSkipsCompletedStations()
{
	std::printf("the Director stops hinting at finished work\n");
	SceneManagement::Scene* scene = MakeScene();

	// Two stations. The Director hints toward the farthest outstanding one, so
	// completing the far station should move hinting to the near one — and
	// completing both should stop it entirely.
	InteractableObject* near = AddStation(scene, Vector3{ 3, 0, 0 }, MINI_GAME_CRANE_ID, 910);
	InteractableObject* far = AddStation(scene, Vector3{ 30, 0, 0 }, MINI_GAME_CRANE_ID, 911);

	auto owned = std::make_unique<Stalker>();
	owned->id = 912;
	owned->rigidBody3D.Teleport(Vector3{ 0, 0, 0 });
	owned->rigidBody3D.isStatic = true;
	scene->gameMap.LoadEntity(std::move(owned));

	// Long enough past kQuietBeforeHint that the Director is willing to speak.
	const auto runDirector = [scene]() {
		scene->director = {};
		for (int i = 0; i < 60; ++i) { scene->soundField.Update(0.5f); }
		scene->director.Update(scene, 30.0f);
		return scene->director.Log();
	};

	auto hints = runDirector();
	Check(!hints.empty(), "an outstanding station draws a hint");
	if (!hints.empty())
	{
		Check(hints.back().region.x > 20.0f, "the hint points at the farthest outstanding station");
	}

	far->isCompleted = true;
	hints = runDirector();
	Check(!hints.empty(), "a remaining station still draws a hint");
	if (!hints.empty())
	{
		Check(hints.back().region.x < 10.0f, "hinting moves on once the far station is done");
	}

	near->isCompleted = true;
	hints = runDirector();
	Check(hints.empty(), "with every station complete the Director says nothing");
	delete scene;
}

int main()
{
	// GameObject's constructor uploads a fallback cube, so a GL context has to
	// exist before any world object is built.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-task-station-tests");

	std::printf("Task station tests\n\n");

	TestStationStartsIncomplete();
	TestWinningCompletesTheStation();
	TestQuittingLeavesTheStationOutstanding();
	TestWinningTogglesTheTarget();
	TestMissingTargetStillCompletes();
	TestPracticeRunCompletesNothing();
	TestUnknownGameStartsNothing();
	TestCompletionRoundTripsThroughJson();
	TestOldSavesKeepTheirTarget();

	TestDirectorSkipsCompletedStations();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
