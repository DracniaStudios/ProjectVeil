/**
 * The render-off tick (MultiplayerSplitPlan.md E3; VerticalSliceDecisions
 * Round 2 standing rule: "every system updates with rendering off, input as
 * commands").
 *
 * Loads the real World save (chunk_1) and runs 10,000 simulation ticks of it
 * with commands only: no frame is begun, nothing is drawn, no input device is
 * read and no camera is touched. The world has to advance anyway: the player
 * walks, the stalker patrols, the sound field and the Director keep time.
 *
 * A hidden window is still opened, because building a GameObject binds a GPU
 * model (the logged R1 debt, review S8, which moves with Step 1.4). The test
 * never calls BeginDrawing, so nothing it runs can depend on a frame.
 */

#include <raylib.h>

#include <Scene.h>
#include <SaveSystem.h>
#include <AI/Stalker.h>
#include <sim/FixedTick.h>
#include <sim/PlayerCommand.h>

#include <chrono>
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

static bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

static Stalker* FindStalker(SceneManagement::Scene& scene)
{
	Stalker* found = nullptr;
	scene.gameMap.ForEachEntity([&](Entity& entity) {
		if (entity.kind == ENTITYKIND_STALKER) { found = static_cast<Stalker*>(&entity); return false; }
		return true;
	});
	return found;
}

static void TestTenThousandTicksWithRenderingOff()
{
	std::printf("10,000 ticks of chunk_1 with rendering off\n");

	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	scene->PlayerInSlot(0)->setSpawnPoint(Vector3(20, 2, 0));
	scene->PlayerInSlot(0)->rigidBody3D.Teleport(Vector3(20, 2, 0));
	const bool loaded = SaveSystem::LoadWorld("chunk_1", *scene);
	Check(loaded, "the World save loads");

	Stalker* stalker = FindStalker(*scene);
	Check(stalker != nullptr, "the World save has a stalker");
	if (!loaded || stalker == nullptr) { SceneManagement::Scene_delete(scene); return; }

	const Vector3 playerStart = scene->PlayerInSlot(0)->getPosition();
	const Vector3 stalkerStart = stalker->getPosition();
	const float soundStart = scene->soundField.Now();
	const float directorStart = scene->director.Now();
	float stalkerTravel = 0.0f;
	Vector3 stalkerLast = stalkerStart;

	const auto started = std::chrono::steady_clock::now();
	constexpr int kTicks = 10000;
	for (int tick = 0; tick < kTicks; ++tick)
	{
		// Walk forward, turning slowly, crouched so the player makes no noise
		// that would pull the stalker off its route
		Sim::PlayerCommand command = {};
		command.sequence = static_cast<std::uint32_t>(tick + 1);
		command.moveY = 1;
		command.buttons = Sim::BUTTON_CROUCH;
		command.yawDelta = 0.002f;
		SceneManagement::Scene_tick(*scene, Sim::kTickDelta, std::span<const Sim::PlayerCommand>(&command, 1));

		// A catch would reload the scene through the SceneManager; this test is
		// about the tick advancing, so keep the stalker away from the player
		if (Stalker* s = FindStalker(*scene))
		{
			stalkerTravel += Vector3Distance(stalkerLast, s->getPosition());
			stalkerLast = s->getPosition();
		}
	}
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
	std::printf("  %d ticks took %.2f s (%.0f ticks/s)\n", kTicks, seconds, kTicks / seconds);

	const float simulated = kTicks * Sim::kTickDelta;
	Check(std::fabs((scene->soundField.Now() - soundStart) - simulated) < 0.5f, "the sound field clock advanced by the simulated time");
	Check(std::fabs((scene->director.Now() - directorStart) - simulated) < 0.5f, "the Director's clock advanced by the simulated time");
	Check(Finite(scene->PlayerInSlot(0)->getPosition()), "the player's position stays finite");
	Check(Vector3Distance(playerStart, scene->PlayerInSlot(0)->getPosition()) > 1.0f || scene->PlayerInSlot(0)->lookYaw != 0.0f, "the player moved under its commands");
	Check(std::fabs(scene->PlayerInSlot(0)->lookYaw) > 0.1f, "the player's look turned under its commands");
	Check(stalkerTravel > 5.0f, "the stalker moved along its route (travelled " + std::to_string(stalkerTravel) + ")");
	if (Stalker* s = FindStalker(*scene)) { Check(Finite(s->getPosition()), "the stalker's position stays finite"); }

	SceneManagement::Scene_delete(scene);
}

int main()
{
	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(64, 64, "veil-render-off-tests");

	std::printf("Render-off tick tests\n\n");

	TestTenThousandTicksWithRenderingOff();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
