/**
 * Tests for the Director: when it hints, where it points, and what it never
 * reads.
 *
 * The Director may read world facts only. The one that is easy to get wrong is
 * the running Task station: it is where the player is standing, so hinting at
 * it would hand the Stalker the player's position under the cover of a world
 * fact. These cases pin that down along with the rest of the rules -- the
 * Stalker must be patrolling, it must have been quiet for kQuietBeforeHint,
 * and hints are spaced by kHintCooldown.
 *
 * TaskStationTests covers completed stations. Like the other engine-linked
 * suites this needs a GL context, since a world object binds a model when it
 * enters the map. See tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <AI/Director.h>
#include <AI/Stalker.h>

#include <cstdio>
#include <cstring>
#include <memory>
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
	return new SceneManagement::Scene();
}

static InteractableObject* AddStation(SceneManagement::Scene* scene, Vector3 at, std::uint64_t id,
                                      InteractionType type = INTERACT_MINIGAME)
{
	auto owned = std::make_unique<InteractableObject>(type, MINI_GAME_CRANE_ID, 0);
	owned->id = id;
	owned->name = "TestStation";
	owned->isEnabled = true;
	owned->isInteractable = true;
	owned->rigidBody3D.Teleport(at);
	InteractableObject* station = owned.get();
	scene->gameMap.RestoreInteractable(std::move(owned));
	return station;
}

static Stalker* AddStalker(SceneManagement::Scene* scene, Vector3 at, std::uint64_t id = 500)
{
	auto owned = std::make_unique<Stalker>();
	owned->id = id;
	owned->rigidBody3D.Teleport(at);
	owned->rigidBody3D.isStatic = true;
	owned->onEnable(); // full health and stamina, as a spawned Stalker
	Stalker* stalker = owned.get();
	scene->gameMap.RestoreEntity(std::move(owned));
	return stalker;
}

// Runs the sound field's clock forward with nothing heard, so the Stalker's
// last stimulus is `seconds` in the past
static void PassQuietTime(SceneManagement::Scene* scene, float seconds)
{
	const int steps = static_cast<int>(seconds / 0.5f) + 1;
	for (int i = 0; i < steps; ++i) { scene->soundField.Update(0.5f); }
}

// ─── When the Director speaks ──────────────────────────────────────────────

static void TestAQuietPatrolDrawsAHint()
{
	std::printf("a patrolling stalker that has heard nothing for a while is given a hint\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStalker(scene, Vector3{ 0, 0, 0 });
	AddStation(scene, Vector3{ 12, 0, 4 }, 900);

	PassQuietTime(scene, Director::kQuietBeforeHint + 1.0f);
	scene->director.Update(scene, 0.1f);

	const auto& log = scene->director.Log();
	Check(log.size() == 1, "exactly one hint is logged");
	if (!log.empty())
	{
		Check(log.back().region.x == 12.0f && log.back().region.z == 4.0f, "the hint names the station's position");
		Check(log.back().confidence == Director::kFixedConfidence, "with the fixed confidence");
		Check(log.back().reason != nullptr && std::strlen(log.back().reason) > 0, "and names the fact behind it");
		Check(log.back().timestamp == scene->director.Now(), "stamped with the Director's own clock");
	}
	delete scene;
}

static void TestARecentNoiseHoldsTheHintBack()
{
	std::printf("a stalker that heard something recently is left to act on it\n");
	SceneManagement::Scene* scene = MakeScene();
	Stalker* stalker = AddStalker(scene, Vector3{ 0, 0, 0 });
	AddStation(scene, Vector3{ 12, 0, 0 }, 900);

	PassQuietTime(scene, 20.0f);
	stalker->lastStimulusTime = scene->soundField.Now() - (Director::kQuietBeforeHint - 1.0f);
	scene->director.Update(scene, 0.1f);
	Check(scene->director.Log().empty(), "no hint inside kQuietBeforeHint of the last stimulus");

	stalker->lastStimulusTime = scene->soundField.Now() - (Director::kQuietBeforeHint + 1.0f);
	scene->director.Update(scene, 0.1f);
	Check(scene->director.Log().size() == 1, "a hint once it has been quiet long enough");
	delete scene;
}

static void TestOnlyAPatrollingStalkerIsAdvised()
{
	std::printf("a stalker that is investigating, hunting or searching is not given hints\n");

	for (StalkerState state : { STALKER_INVESTIGATE, STALKER_HUNT, STALKER_SEARCH_LAST_KNOWN })
	{
		SceneManagement::Scene* scene = MakeScene();
		Stalker* stalker = AddStalker(scene, Vector3{ 0, 0, 0 });
		AddStation(scene, Vector3{ 12, 0, 0 }, 900);
		stalker->state = state;

		PassQuietTime(scene, 20.0f);
		scene->director.Update(scene, 0.1f);
		Check(scene->director.Log().empty(), std::string("no hint while ") + stalkerStateToString(state));
		delete scene;
	}
}

static void TestNoStalkerMeansNoHint()
{
	std::printf("with no stalker in the world there is no one to advise\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStation(scene, Vector3{ 12, 0, 0 }, 900);

	PassQuietTime(scene, 20.0f);
	scene->director.Update(scene, 2.0f);
	scene->director.Update(scene, 3.0f);

	Check(scene->director.Log().empty(), "no hint is logged");
	Check(scene->director.Now() == 5.0f, "the Director's clock still runs");
	delete scene;
}

static void TestNoSceneIsIgnored()
{
	std::printf("updating with no scene does nothing\n");
	Director director;

	director.Update(nullptr, 1.0f);
	Check(director.Log().empty(), "no hint");
	Check(director.Now() == 0.0f, "and the clock has not moved");
}

// ─── Where the Director points ─────────────────────────────────────────────

static void TestTheRunningStationIsNeverHinted()
{
	std::printf("the station the player is using is never hinted, since that is where the player is\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStalker(scene, Vector3{ 0, 0, 0 });
	InteractableObject* running = AddStation(scene, Vector3{ 40, 0, 0 }, 900);
	AddStation(scene, Vector3{ 10, 0, 0 }, 901);
	running->isRunningMiniGame = true;

	PassQuietTime(scene, 20.0f);
	scene->director.Update(scene, 0.1f);

	const auto& log = scene->director.Log();
	Check(log.size() == 1, "the idle station still draws a hint");
	if (!log.empty())
	{
		Check(log.back().region.x == 10.0f, "and it is the idle station, though the running one is farther");
	}

	// With the only remaining station the running one, the Director stays quiet
	SceneManagement::Scene* alone = MakeScene();
	AddStalker(alone, Vector3{ 0, 0, 0 });
	AddStation(alone, Vector3{ 40, 0, 0 }, 900)->isRunningMiniGame = true;
	PassQuietTime(alone, 20.0f);
	alone->director.Update(alone, 0.1f);
	Check(alone->director.Log().empty(), "a running station alone draws no hint");

	delete alone;
	delete scene;
}

static void TestTheFarthestOutstandingStationIsChosen()
{
	std::printf("the hint points at the outstanding station farthest from the stalker\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStalker(scene, Vector3{ 5, 0, 5 });
	AddStation(scene, Vector3{ 8, 0, 5 }, 900);
	AddStation(scene, Vector3{ 5, 0, -20 }, 901);
	AddStation(scene, Vector3{ -6, 0, 5 }, 902);

	PassQuietTime(scene, 20.0f);
	scene->director.Update(scene, 0.1f);

	const auto& log = scene->director.Log();
	Check(log.size() == 1, "one hint is logged");
	if (!log.empty())
	{
		Check(log.back().region.z == -20.0f, "it names the station 25 units away, not the nearer ones");
	}
	delete scene;
}

static void TestOnlyInteractableMiniGameStationsCount()
{
	std::printf("items, unlocks and stations that cannot be used are not objectives\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStalker(scene, Vector3{ 0, 0, 0 });
	AddStation(scene, Vector3{ 50, 0, 0 }, 900, INTERACT_ITEM);
	AddStation(scene, Vector3{ -50, 0, 0 }, 901, INTERACT_UNLOCK);
	AddStation(scene, Vector3{ 0, 0, 50 }, 902)->isInteractable = false;

	PassQuietTime(scene, 20.0f);
	scene->director.Update(scene, 0.1f);
	Check(scene->director.Log().empty(), "none of them draws a hint");

	AddStation(scene, Vector3{ 3, 0, 0 }, 903);
	scene->director.Update(scene, 0.1f);
	Check(scene->director.Log().size() == 1, "a usable Task station does");
	if (!scene->director.Log().empty())
	{
		Check(scene->director.Log().back().region.x == 3.0f, "and the hint is for it, not the farther non-objectives");
	}
	delete scene;
}

// ─── Pacing ────────────────────────────────────────────────────────────────

static void TestHintsAreSpacedByTheCooldown()
{
	std::printf("a second hint waits out the cooldown\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStalker(scene, Vector3{ 0, 0, 0 });
	AddStation(scene, Vector3{ 12, 0, 0 }, 900);
	PassQuietTime(scene, 20.0f);

	scene->director.Update(scene, 0.1f);
	Check(scene->director.Log().size() == 1, "the first hint is given");
	Check(scene->director.CooldownRemaining() == Director::kHintCooldown, "and starts the cooldown");

	// Just short of the cooldown, a frame at a time
	const int frames = static_cast<int>((Director::kHintCooldown - 1.0f) / 0.5f);
	for (int i = 0; i < frames; ++i) { scene->director.Update(scene, 0.5f); }
	Check(scene->director.Log().size() == 1, "no second hint inside the cooldown");

	for (int i = 0; i < 4; ++i) { scene->director.Update(scene, 0.5f); }
	Check(scene->director.Log().size() == 2, "a second hint once the cooldown has passed");
	delete scene;
}

static void TestTheLogKeepsTheMostRecentHints()
{
	std::printf("the log keeps the most recent kLogCapacity hints\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStalker(scene, Vector3{ 0, 0, 0 });
	AddStation(scene, Vector3{ 12, 0, 0 }, 900);
	PassQuietTime(scene, 20.0f);

	const int hints = static_cast<int>(Director::kLogCapacity) + 5;
	for (int i = 0; i < hints; ++i) { scene->director.Update(scene, Director::kHintCooldown + 0.1f); }

	const auto& log = scene->director.Log();
	Check(log.size() == Director::kLogCapacity, "the log stops growing at its capacity");
	if (log.size() >= 2)
	{
		Check(log.front().timestamp < log.back().timestamp, "it reads oldest first");
		Check(log.back().timestamp == scene->director.Now(), "the newest hint is last");
		Check(log.front().timestamp > (Director::kHintCooldown + 0.1f) * 5.0f, "the oldest hints were the ones dropped");
	}
	delete scene;
}

static void TestClearForgetsEverything()
{
	std::printf("Clear forgets the log, the cooldown and the clock\n");
	SceneManagement::Scene* scene = MakeScene();
	AddStalker(scene, Vector3{ 0, 0, 0 });
	AddStation(scene, Vector3{ 12, 0, 0 }, 900);
	PassQuietTime(scene, 20.0f);

	scene->director.Update(scene, 3.0f);
	Check(!scene->director.Log().empty() && scene->director.CooldownRemaining() > 0.0f, "a hint was given");

	scene->director.Clear();
	Check(scene->director.Log().empty(), "the log is empty");
	Check(scene->director.CooldownRemaining() == 0.0f, "the cooldown is over");
	Check(scene->director.Now() == 0.0f, "the clock is back at zero");

	scene->director.Update(scene, 0.1f);
	Check(scene->director.Log().size() == 1, "and the next quiet update hints at once");
	delete scene;
}

int main()
{
	// A world object binds a model when it enters the map, so a GL context has
	// to exist before any is added.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-director-tests");

	std::printf("Director tests\n\n");

	TestAQuietPatrolDrawsAHint();
	TestARecentNoiseHoldsTheHintBack();
	TestOnlyAPatrollingStalkerIsAdvised();
	TestNoStalkerMeansNoHint();
	TestNoSceneIsIgnored();

	TestTheRunningStationIsNeverHinted();
	TestTheFarthestOutstandingStationIsChosen();
	TestOnlyInteractableMiniGameStationsCount();

	TestHintsAreSpacedByTheCooldown();
	TestTheLogKeepsTheMostRecentHints();
	TestClearForgetsEverything();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
