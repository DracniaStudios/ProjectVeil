/**
 * Tests for the perception layer's emitters.
 *
 * SoundFieldTests covers the field itself — attenuation, expiry, ranking. This
 * covers the other half of the seam: whether the things in the world that are
 * supposed to make noise actually put an event into that field, at the right
 * place, with the right loudness, and no more often than they should.
 *
 * Footsteps are already exercised indirectly (Player::update3D drives them and
 * the FSM tests depend on the same path), so these cover the three emitters
 * wired later: the running task station, tampering, and physics impacts.
 *
 * Like StalkerFsmTests this links against the game's object files and needs a
 * GL context for GameObject's fallback cube. See tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <Perception/SoundField.h>

#include <cmath>
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

static void CheckNear(float actual, float expected, float tolerance, const std::string& what)
{
	++g_checks;
	if (std::fabs(actual - expected) > tolerance)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected ~%.4f, got %.4f)\n", what.c_str(), expected, actual);
	}
}

static Scene* MakeScene()
{
	Scene* scene = new Scene();
	SceneManager::getInstance().currentScene = scene;
	return scene;
}

// A task station at a known position. Registered directly rather than through
// SpawnInteractable so the id is predictable and the assertions can name it.
static InteractableObject* AddStation(Scene* scene, Vector3 at, std::uint64_t id = 700)
{
	auto owned = std::make_unique<InteractableObject>(INTERACT_MINIGAME, MINI_GAME_FLAPPY_BIRD_ID, 0);
	owned->id = id;
	owned->name = "TestStation";
	owned->isEnabled = true;
	owned->isInteractable = true;
	owned->rigidBody3D.Teleport(at);
	InteractableObject* station = owned.get();
	scene->gameMap.LoadInteractable(std::move(owned));
	return station;
}

// Counts only events of one kind, so a case is never accidentally satisfied by
// a different emitter's noise.
static int CountOfKind(const Scene* scene, SoundKind kind)
{
	int count = 0;
	for (const auto& event : scene->soundField.Events())
	{
		if (event.kind == kind) { ++count; }
	}
	return count;
}

static const SoundEvent* FirstOfKind(const Scene* scene, SoundKind kind)
{
	for (const auto& event : scene->soundField.Events())
	{
		if (event.kind == kind) { return &event; }
	}
	return nullptr;
}

// ─── Station ───────────────────────────────────────────────────────────────

static void TestIdleStationIsSilent()
{
	std::printf("an idle station makes no noise\n");
	Scene* scene = MakeScene();
	AddStation(scene, Vector3{ 5, 0, 5 });

	for (int i = 0; i < 120; ++i) { scene->EmitStationNoise(0.016f); }

	Check(CountOfKind(scene, SOUND_STATION) == 0,
		"a station nobody is using stays quiet");
	delete scene;
}

static void TestRunningStationEmitsAtTheStation()
{
	std::printf("a running station emits at its own position\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 5, 0, 5 });
	station->isRunningMiniGame = true;

	scene->EmitStationNoise(0.016f);

	const SoundEvent* event = FirstOfKind(scene, SOUND_STATION);
	Check(event != nullptr, "starting a station is immediately audible");
	if (event == nullptr) { delete scene; return; }

	// The whole point of the emitter living on the station rather than on the
	// player: if this ever reads as the player's position, the sound-only
	// decision has been broken by the back door.
	CheckNear(event->position.x, 5.0f, 0.001f, "the noise is at the station, not the player");
	CheckNear(event->position.z, 5.0f, 0.001f, "the noise is at the station, not the player");
	CheckNear(event->loudness, 0.6f, 0.001f, "station loudness matches the plan's table");
	Check(event->sourceId == station->id, "the noise is attributed to the station");
	delete scene;
}

static void TestStationNoiseIsPeriodicNotPerFrame()
{
	std::printf("station noise is periodic, not per-frame\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 });
	station->isRunningMiniGame = true;

	// Two seconds at 60fps. A per-frame emitter would produce 120 events and
	// swamp the 64-slot ring; the interval should yield a handful.
	for (int i = 0; i < 125; ++i)
	{
		scene->soundField.Update(0.016f);
		scene->EmitStationNoise(0.016f);
	}

	const int emitted = CountOfKind(scene, SOUND_STATION);
	Check(emitted >= 2 && emitted <= 4,
		"two seconds of running yields a few hums, not one per frame");
	delete scene;
}

static void TestReleasingTheMiniGameStopsTheHum()
{
	std::printf("releasing the minigame frees the station\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 });
	station->isRunningMiniGame = true;

	Check(scene->GetRunningStation() == station, "the station reports as occupied");

	// ReleaseMiniGame is the single point that knows the minigame ended. Before
	// this fix isRunningMiniGame was a one-way latch, so the station hummed for
	// the rest of the session and the Director never hinted at it again.
	scene->ReleaseMiniGame();

	Check(scene->GetRunningStation() == nullptr, "releasing frees the station");
	Check(!station->isRunningMiniGame, "the occupied flag is cleared");

	scene->soundField.Clear();
	for (int i = 0; i < 120; ++i) { scene->EmitStationNoise(0.016f); }
	Check(CountOfKind(scene, SOUND_STATION) == 0, "the hum stops once the station is free");
	delete scene;
}

static void TestDisabledStationIsSilent()
{
	std::printf("a disabled station makes no noise\n");
	Scene* scene = MakeScene();
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 });
	station->isRunningMiniGame = true;
	station->isEnabled = false;

	for (int i = 0; i < 120; ++i) { scene->EmitStationNoise(0.016f); }

	Check(CountOfKind(scene, SOUND_STATION) == 0,
		"a station switched off in the world does not hum");
	delete scene;
}

// ─── Tampering ─────────────────────────────────────────────────────────────

static void TestInteractingWithAStationIsTampering()
{
	std::printf("working a station emits a tamper noise\n");
	Scene* scene = MakeScene();
	scene->player = new Player();
	scene->player->id = PLAYER_ID;
	scene->player->artifactUnlocked = MINI_GAME_RO_SHAM_BOO_ID;   // everything unlocked

	InteractableObject* station = AddStation(scene, Vector3{ 3, 0, -4 });
	station->onInteract();

	const SoundEvent* event = FirstOfKind(scene, SOUND_TAMPER);
	Check(event != nullptr, "interacting with a station is audible");
	if (event == nullptr) { delete scene; return; }

	CheckNear(event->loudness, 0.8f, 0.001f, "tamper loudness matches the plan's table");
	CheckNear(event->position.x, 3.0f, 0.001f, "the noise is at the station");
	CheckNear(event->position.z, -4.0f, 0.001f, "the noise is at the station");
	Check(event->sourceId == station->id, "the noise is attributed to the station");
	Check(CountOfKind(scene, SOUND_TAMPER) == 1, "tampering is one-shot, not continuous");
	delete scene;
}

static void TestLockedStationStillMakesNoise()
{
	std::printf("a failed attempt is still audible\n");
	Scene* scene = MakeScene();
	scene->player = new Player();
	scene->player->id = PLAYER_ID;
	scene->player->artifactUnlocked = MINI_GAME_FLAPPY_BIRD_ID;

	// A variation the player has not unlocked: ActivateMiniGame bails before
	// starting anything. Rattling a station you cannot open is still
	// interference, and a silent failed attempt would be the safest way to play.
	InteractableObject* station = AddStation(scene, Vector3{ 0, 0, 0 });
	station->variation = MINI_GAME_RO_SHAM_BOO_ID;
	station->onInteract();

	Check(CountOfKind(scene, SOUND_TAMPER) == 1, "a locked station still makes noise");
	Check(scene->GetRunningStation() == nullptr, "but no minigame actually started");
	delete scene;
}

// ─── Physics impacts ───────────────────────────────────────────────────────

// Two bodies overlapping along X, closing at `closingSpeed`. Returns the
// striker so the caller can check attribution.
static GameObject* AddCrate(Scene* scene, Vector3 at, Vector3 velocity,
                            bool isStatic, std::uint64_t id)
{
	GameObject crate = {};
	crate.id = id;
	crate.name = "TestCrate";
	crate.isEnabled = true;
	crate.rigidBody3D.isStatic = isStatic;
	crate.rigidBody3D.canCollide = true;
	crate.rigidBody3D.translation = at;
	crate.rigidBody3D.scale = Vector3{ 1, 1, 1 };
	crate.rigidBody3D.SetVelocity(velocity);
	crate.rigidBody3D.SyncBroadPhaseBox();
	return scene->gameMap.LoadGameObject(crate);
}

static void TestFastImpactIsAudible()
{
	std::printf("a fast impact is audible\n");
	Scene* scene = MakeScene();

	// Overlapping boxes, the second driving into the first hard.
	GameObject* wall = AddCrate(scene, Vector3{ 0, 0, 0 }, Vector3Zero(), true, 800);
	GameObject* crate = AddCrate(scene, Vector3{ 0.5f, 0, 0 }, Vector3{ -10, 0, 0 }, false, 801);

	wall->rigidBody3D.resolveConstrains(wall, crate);

	const SoundEvent* event = FirstOfKind(scene, SOUND_IMPACT);
	Check(event != nullptr, "a hard collision emits an impact");
	if (event == nullptr) { delete scene; return; }

	Check(event->loudness > 0.5f, "a 10 u/s impact is loud");

	// solveCollision visits each pair once and the wall happened to be first.
	// Attribution has to follow the body that moved, or the noise would be
	// credited to the static geometry that was standing still.
	Check(event->sourceId == crate->id, "the impact is attributed to the moving body");
	delete scene;
}

static void TestRestingContactIsSilent()
{
	std::printf("a resting contact is silent\n");
	Scene* scene = MakeScene();

	// Overlapping but barely closing — a body settled against another. This is
	// the case that decides whether walking a room floods the sound field.
	GameObject* floor = AddCrate(scene, Vector3{ 0, 0, 0 }, Vector3Zero(), true, 810);
	GameObject* box = AddCrate(scene, Vector3{ 0.5f, 0, 0 }, Vector3{ -0.2f, 0, 0 }, false, 811);

	floor->rigidBody3D.resolveConstrains(floor, box);

	Check(CountOfKind(scene, SOUND_IMPACT) == 0,
		"a slow settling contact makes no perception event");
	delete scene;
}

static void TestImpactScalesWithClosingSpeed()
{
	std::printf("impact loudness scales with closing speed\n");

	const auto loudnessAtSpeed = [](float speed) -> float {
		Scene* scene = MakeScene();
		GameObject* wall = AddCrate(scene, Vector3{ 0, 0, 0 }, Vector3Zero(), true, 820);
		GameObject* crate = AddCrate(scene, Vector3{ 0.5f, 0, 0 }, Vector3{ -speed, 0, 0 }, false, 821);
		wall->rigidBody3D.resolveConstrains(wall, crate);
		const SoundEvent* event = FirstOfKind(scene, SOUND_IMPACT);
		const float loudness = event != nullptr ? event->loudness : 0.0f;
		delete scene;
		return loudness;
	};

	const float gentle = loudnessAtSpeed(4.0f);
	const float hard = loudnessAtSpeed(9.0f);

	Check(gentle > 0.0f, "a moderate impact is audible at all");
	Check(hard > gentle, "a harder impact is louder");
	Check(hard <= 1.0f, "loudness stays within the field's 0..1 contract");
}

static void TestImpactIsOncePerContact()
{
	std::printf("one collision is one noise\n");
	Scene* scene = MakeScene();

	GameObject* wall = AddCrate(scene, Vector3{ 0, 0, 0 }, Vector3Zero(), true, 830);
	GameObject* crate = AddCrate(scene, Vector3{ 0.5f, 0, 0 }, Vector3{ -10, 0, 0 }, false, 831);

	// The solver runs up to eight iterations per frame over the same pair. The
	// emitter hangs off the first-contact branch precisely so that is one bang,
	// not eight.
	for (int i = 0; i < 8; ++i) { wall->rigidBody3D.resolveConstrains(wall, crate); }

	Check(CountOfKind(scene, SOUND_IMPACT) == 1,
		"eight solver iterations over one contact emit one noise");
	delete scene;
}

int main()
{
	// GameObject's constructor uploads a fallback cube, so a GL context has to
	// exist before any world object is built.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-emitter-tests");

	std::printf("Perception emitter tests\n\n");

	TestIdleStationIsSilent();
	TestRunningStationEmitsAtTheStation();
	TestStationNoiseIsPeriodicNotPerFrame();
	TestReleasingTheMiniGameStopsTheHum();
	TestDisabledStationIsSilent();

	TestInteractingWithAStationIsTampering();
	TestLockedStationStillMakesNoise();

	TestFastImpactIsAudible();
	TestRestingContactIsSilent();
	TestImpactScalesWithClosingSpeed();
	TestImpactIsOncePerContact();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
