/**
 * Tests for task-station state: per-station completion, and the Scene's
 * two-deep record of minigame progress.
 *
 * Both exist because progress used to have nowhere to live. MiniGameData is
 * heap-allocated by the MiniGame constructor and deleted by ReleaseMiniGame, so
 * a score lasted exactly as long as the overlay showing it, and a station had no
 * way to say it had ever been finished. The Director reads the completion flag
 * to tell an outstanding objective from a done one, so a regression here is a
 * regression in what the AI applies pressure toward — not just bookkeeping.
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

static Scene* MakeScene()
{
	Scene* scene = new Scene();
	SceneManager::getInstance().currentScene = scene;
	scene->player = new Player();
	scene->player->id = PLAYER_ID;
	// Everything unlocked, so ActivateMiniGame never bails at the unlock gate
	// and these cases are about station state rather than progression.
	scene->player->artifactUnlocked = MINI_GAME_RO_SHAM_BOO_ID;
	return scene;
}

static InteractableObject* AddStation(Scene* scene, Vector3 at, int miniGameId,
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
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 900);

	Check(!station->isCompleted, "a placed station starts outstanding");
	delete scene;
}

static void TestCompletingMarksTheRunningStation()
{
	std::printf("finishing a minigame completes its station\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 901);
	station->onInteract();

	Check(scene->GetRunningStation() == station, "the station is occupied while it runs");

	MiniGameData data = {};
	data.score = 5;
	data.scoreGoal = 3;
	CompleteMiniGame(data, *scene->player, scene->gameMap, BUFF_RANGE);

	Check(station->isCompleted, "carrying the score goal completes the station");
	delete scene;
}

static void TestUnfinishedWorkDoesNotComplete()
{
	std::printf("falling short leaves the station outstanding\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 902);
	station->onInteract();

	MiniGameData data = {};
	data.score = 1;
	data.scoreGoal = 3;
	CompleteMiniGame(data, *scene->player, scene->gameMap, BUFF_RANGE);

	Check(!station->isCompleted, "a score below the goal does not complete the station");
	delete scene;
}

static void TestCompletionSurvivesRelease()
{
	std::printf("completion outlives the minigame that earned it\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 }, MINI_GAME_CRANE_ID, 903);
	station->onInteract();

	MiniGameData data = {};
	data.score = 5;
	data.scoreGoal = 3;
	CompleteMiniGame(data, *scene->player, scene->gameMap, BUFF_RANGE);
	scene->ReleaseMiniGame();

	// isRunningMiniGame is "occupied right now" and clears on release;
	// isCompleted is "this work is done" and must not.
	Check(!station->isRunningMiniGame, "releasing frees the station");
	Check(station->isCompleted, "but the station stays completed");
	delete scene;
}

static void TestCompletionRoundTripsThroughJson()
{
	std::printf("completion survives a save/load round trip\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 2, 0, 3 }, MINI_GAME_CRANE_ID, 904);
	station->isCompleted = true;
	station->isRunningMiniGame = true;

	Json saved = station->formatToJson();

	InteractableObject loaded;
	Check(loaded.loadFromJson(saved), "the station loads back");
	Check(loaded.isCompleted, "completion is persisted");
	// Occupancy is a live fact about someone standing there, and a reloaded save
	// has nobody at the station.
	Check(!loaded.isRunningMiniGame, "occupancy is not persisted");
	delete scene;
}

// ─── Director reads completion ─────────────────────────────────────────────

static void TestDirectorSkipsCompletedStations()
{
	std::printf("the Director stops hinting at finished work\n");
	Scene* scene = MakeScene();

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

// ─── MiniGameData history ──────────────────────────────────────────────────

static void TestNoHistoryBeforeAnythingRuns()
{
	std::printf("a fresh scene has no minigame history\n");
	Scene* scene = MakeScene();

	// A default MiniGameData reads as a genuine score of zero, which is why the
	// flags exist at all.
	Check(!scene->hasCurrentMiniGameData, "no current data before anything runs");
	Check(!scene->hasPreviousMiniGameData, "no previous data before anything runs");
	delete scene;
}

static void TestProgressIsRecordedOnRelease()
{
	std::printf("progress is captured before the data is freed\n");
	Scene* scene = MakeScene();
	scene->SetMiniGame(MINI_GAME_CRANE_ID);

	Check(scene->miniGame != nullptr, "a minigame started");
	if (scene->miniGame == nullptr) { delete scene; return; }

	// Play a little, then walk away. Before this record existed the score died
	// with the allocation and there was nothing left to resume from.
	scene->miniGame->data->score = 7;
	scene->ReleaseMiniGame();

	Check(scene->hasCurrentMiniGameData, "leaving a station records its progress");
	Check(scene->currentMiniGameData.score == 7, "the recorded score is the one reached");
	delete scene;
}

static void TestSetMiniGameRollsTheHistory()
{
	std::printf("starting a new minigame displaces the old one\n");
	Scene* scene = MakeScene();

	scene->SetMiniGame(MINI_GAME_CRANE_ID);
	scene->miniGame->data->score = 4;

	scene->SetMiniGame(MINI_GAME_MAZE_ID);

	Check(scene->hasPreviousMiniGameData, "the displaced game is remembered");
	Check(scene->previousMiniGameData.score == 4,
		"previous holds the score the displaced game reached");
	Check(scene->hasCurrentMiniGameData, "the incoming game is recorded");
	Check(scene->currentMiniGameData.score == 0, "the incoming game starts fresh");
	delete scene;
}

static void TestHistoryIsTwoDeep()
{
	std::printf("the history is two deep\n");
	Scene* scene = MakeScene();

	scene->SetMiniGame(MINI_GAME_CRANE_ID);
	scene->miniGame->data->score = 1;
	scene->SetMiniGame(MINI_GAME_MAZE_ID);
	scene->miniGame->data->score = 2;
	scene->SetMiniGame(MINI_GAME_DOCTOR_ID);

	// The first game has fallen off the end; only the last two are kept.
	Check(scene->previousMiniGameData.score == 2, "previous is the game just displaced");
	Check(scene->currentMiniGameData.score == 0, "current is the game now running");
	delete scene;
}

static void TestRejectedSetMiniGameLeavesHistoryAlone()
{
	std::printf("an unknown minigame id does not disturb the history\n");
	Scene* scene = MakeScene();

	scene->SetMiniGame(MINI_GAME_CRANE_ID);
	scene->miniGame->data->score = 3;
	scene->SnapshotMiniGameData();

	// SetMiniGame rejects an unknown id before touching anything. A rejected
	// call must not shuffle the history, or a typo would silently discard the
	// record of what the player was doing.
	scene->SetMiniGame(999);

	Check(scene->currentMiniGameData.score == 3, "the current record is untouched");
	Check(!scene->hasPreviousMiniGameData, "nothing was rolled into previous");
	Check(scene->miniGame != nullptr, "the running minigame is untouched");
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
	TestCompletingMarksTheRunningStation();
	TestUnfinishedWorkDoesNotComplete();
	TestCompletionSurvivesRelease();
	TestCompletionRoundTripsThroughJson();

	TestDirectorSkipsCompletedStations();

	TestNoHistoryBeforeAnythingRuns();
	TestProgressIsRecordedOnRelease();
	TestSetMiniGameRollsTheHistory();
	TestHistoryIsTwoDeep();
	TestRejectedSetMiniGameLeavesHistoryAlone();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
