/**
 * The fixed 60 Hz tick and PlayerCommand input (MultiplayerSplitPlan.md E1, E2;
 * DedicatedServerPlan.md tests T1.3 and T1.4).
 *
 *   T1.3  Feel: walking for the same two seconds of real time covers the same
 *         distance at 30, 60 and 144 frames a second (within 2%).
 *   T1.4  Press edges: one press fires exactly once, whether its frame runs no
 *         tick (144 FPS) or two (30 FPS).
 *
 * Frames are simulated rather than rendered: each "frame" samples input once
 * through CommandSampler::Record, asks the FixedTickClock how many ticks it
 * runs, and drives Player::update3D with one consumed command per tick, which
 * is exactly what update_game does. Links the engine like the other
 * engine-tier suites, because a Player needs a Scene and a GL context.
 */

#include <raylib.h>

#include <Scene.h>
#include <Client/CommandSampler.h>
#include <sim/FixedTick.h>
#include <sim/PlayerCommand.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <Client/GamePresentation.h>
#include "TestWindow.h"

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

// A player on no floor, with gravity off: only walking moves it
static Player* MakePlayer()
{
	Player* player = new Player;
	player->type = OBJECT_PLAYER;
	player->onEnable();
	player->rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f);
	player->rigidBody3D.Teleport(Vector3{ 0, 0, 0 });
	return player;
}

static void TestClockRunsSixtyTicksASecond()
{
	std::printf("the clock runs 60 ticks per second of frame time\n");

	for (const float fps : { 30.0f, 60.0f, 144.0f, 240.0f })
	{
		Sim::FixedTickClock clock;
		int ticks = 0;
		const int frames = static_cast<int>(std::lround(fps * 2.0f));
		for (int i = 0; i < frames; ++i) { ticks += clock.Advance(1.0f / fps); }
		Check(std::abs(ticks - 120) <= 1, "two seconds at " + std::to_string(static_cast<int>(fps)) + " FPS run 120 ticks (ran " + std::to_string(ticks) + ")");
	}

	Sim::FixedTickClock clock;
	Check(clock.Advance(1.0f / 30.0f) == 2, "a 30 FPS frame runs two ticks");

	Sim::FixedTickClock stalled;
	Check(stalled.Advance(5.0f) == Sim::kMaxTicksPerFrame, "a long stall runs at most kMaxTicksPerFrame ticks");
	Check(stalled.Advance(1.0f / 60.0f) <= 1, "and the rest of the stall is dropped, not queued");

	Sim::FixedTickClock idle;
	Check(idle.Advance(0.0f) == 0 && idle.Advance(-1.0f) == 0 && idle.Advance(NAN) == 0, "zero, negative or NaN frame time runs nothing");
}

// Walks forward for two seconds of frame time at `fps`; returns the distance
static float WalkFor(float fps)
{
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	Player* player = MakePlayer();
	auto& sampler = Client::CommandSampler::getInstance();
	sampler.Clear();

	Sim::FixedTickClock clock;
	std::uint32_t tick = 0;
	const int frames = static_cast<int>(std::lround(fps * 2.0f));
	for (int i = 0; i < frames; ++i)
	{
		sampler.SetLookBase(player->lookYaw, player->lookPitch);
		sampler.Record(0, 1, 0, 0, 0.0f, 0.0f);
		const int ticks = clock.Advance(1.0f / fps);
		for (int t = 0; t < ticks; ++t)
		{
			player->update3D(*scene, sampler.Consume(++tick), Sim::kTickDelta);
		}
	}

	const Vector3 at = player->getPosition();
	const float distance = std::sqrt(at.x * at.x + at.z * at.z);
	delete player;
	delete scene;
	return distance;
}

static void TestFeelIsFrameRateIndependent()
{
	std::printf("T1.3: the same walk covers the same distance at 30, 60 and 144 FPS\n");

	const float at60 = WalkFor(60.0f);
	Check(at60 > 1.0f, "walking forward moves the player");

	for (const float fps : { 30.0f, 144.0f })
	{
		const float distance = WalkFor(fps);
		const float ratio = distance / at60;
		Check(ratio > 0.98f && ratio < 1.02f, "the walk at " + std::to_string(static_cast<int>(fps)) + " FPS is within 2% of 60 FPS (ratio " + std::to_string(ratio) + ")");
	}
}

// Presses `button` on one frame, then runs `frames` more frames at `fps` with
// nothing pressed. Returns how many ticks carried the press.
static int TicksCarryingPress(float fps, Sim::CommandButton button, int frames, int* ticksRun = nullptr)
{
	auto& sampler = Client::CommandSampler::getInstance();
	sampler.Clear();
	Sim::FixedTickClock clock;
	std::uint32_t tick = 0;
	int carried = 0;
	int total = 0;

	for (int i = 0; i <= frames; ++i)
	{
		sampler.Record(0, 0, 0, i == 0 ? static_cast<std::uint16_t>(button) : 0, 0.0f, 0.0f);
		const int ticks = clock.Advance(1.0f / fps);
		for (int t = 0; t < ticks; ++t)
		{
			if (Sim::HasButton(sampler.Consume(++tick), button)) { ++carried; }
			++total;
		}
	}
	if (ticksRun) { *ticksRun = total; }
	return carried;
}

static void TestPressesFireOnce()
{
	std::printf("T1.4: a press fires exactly once at 30 and 144 FPS\n");

	for (const float fps : { 30.0f, 144.0f })
	{
		const std::string at = " at " + std::to_string(static_cast<int>(fps)) + " FPS";
		for (const auto button : { Sim::BUTTON_INTERACT, Sim::BUTTON_JUMP, Sim::BUTTON_FLASHLIGHT })
		{
			int ticks = 0;
			Check(TicksCarryingPress(fps, button, 30, &ticks) == 1, "button " + std::to_string(button) + " is carried by exactly one tick" + at);
			Check(ticks > 0, "and ticks ran" + at);
		}
	}

	// A frame that runs no tick keeps the press for the next one
	auto& sampler = Client::CommandSampler::getInstance();
	sampler.Clear();
	sampler.Record(0, 0, 0, Sim::BUTTON_JUMP, 0.0f, 0.0f);
	sampler.Record(0, 0, 0, 0, 0.0f, 0.0f); // a second frame, still no tick
	Check(Sim::HasButton(sampler.Consume(1), Sim::BUTTON_JUMP), "a press survives frames that run no tick");
	Check(!Sim::HasButton(sampler.Consume(2), Sim::BUTTON_JUMP), "and is gone once a tick has it");

	// Held buttons are not edges: they ride every tick while down
	sampler.Clear();
	sampler.Record(0, 0, Sim::BUTTON_SPRINT, 0, 0.0f, 0.0f);
	Check(Sim::HasButton(sampler.Consume(3), Sim::BUTTON_SPRINT) && Sim::HasButton(sampler.Consume(4), Sim::BUTTON_SPRINT), "a held button is in every tick while it is down");
}

static void TestFlashlightTogglesOncePerPress()
{
	std::printf("T1.4: the flashlight toggles once per press, through Player::update3D\n");

	for (const float fps : { 30.0f, 144.0f })
	{
		SceneManagement::Scene* scene = new SceneManagement::Scene();
		Player* player = MakePlayer();
		auto& sampler = Client::CommandSampler::getInstance();
		sampler.Clear();
		Sim::FixedTickClock clock;
		std::uint32_t tick = 0;
		int toggles = 0;
		bool last = player->flashlightOn;

		for (int i = 0; i < 40; ++i)
		{
			sampler.Record(0, 0, 0, i == 0 ? Sim::BUTTON_FLASHLIGHT : 0, 0.0f, 0.0f);
			const int ticks = clock.Advance(1.0f / fps);
			for (int t = 0; t < ticks; ++t)
			{
				player->update3D(*scene, sampler.Consume(++tick), Sim::kTickDelta);
				if (player->flashlightOn != last) { ++toggles; last = player->flashlightOn; }
			}
		}
		Check(toggles == 1, "one press toggles the flashlight once at " + std::to_string(static_cast<int>(fps)) + " FPS");
		delete player;
		delete scene;
	}
}

static void TestLookFollowsCommands()
{
	std::printf("look turns only by command, clamped\n");

	SceneManagement::Scene* scene = new SceneManagement::Scene();
	Player* player = MakePlayer();

	Sim::PlayerCommand turn = {};
	turn.yawDelta = 0.5f;
	player->update3D(*scene, turn, Sim::kTickDelta);
	Check(std::abs(player->lookYaw - 0.5f) < 1e-5f, "a yaw delta turns the player");
	Check(std::abs(player->rigidBody3D.forward.x - std::sin(0.5f)) < 1e-4f, "and the body faces the new yaw");

	Sim::PlayerCommand up = {};
	up.pitchDelta = 10.0f;
	player->update3D(*scene, up, Sim::kTickDelta);
	Check(player->lookPitch <= Player::kPitchLimit + 1e-5f, "pitch never passes the limit");

	Sim::PlayerCommand bad = {};
	bad.yawDelta = NAN;
	bad.pitchDelta = INFINITY;
	const float yawBefore = player->lookYaw;
	player->update3D(*scene, bad, Sim::kTickDelta);
	Check(std::isfinite(player->lookYaw) && std::isfinite(player->lookPitch) && player->lookYaw == yawBefore, "a NaN or infinite delta is ignored");

	// The sampler sends only the pitch the view can still turn
	auto& sampler = Client::CommandSampler::getInstance();
	sampler.Clear();
	sampler.SetLookBase(0.0f, Player::kPitchLimit - 0.1f);
	sampler.Record(0, 0, 0, 0, 0.0f, -1000.0f); // a big upward mouse move
	const Sim::PlayerCommand clamped = sampler.Consume(1);
	Check(std::abs(clamped.pitchDelta - 0.1f) < 1e-4f, "the sampler clamps pitch against the look base");

	// A repeated command (rule P-6) keeps movement and held buttons, drops the rest
	Sim::PlayerCommand last = {};
	last.moveY = 1;
	last.buttons = Sim::BUTTON_SPRINT | Sim::BUTTON_JUMP;
	last.yawDelta = 0.3f;
	const Sim::PlayerCommand repeat = Sim::RepeatOf(last);
	Check(repeat.moveY == 1 && Sim::HasButton(repeat, Sim::BUTTON_SPRINT), "a repeat keeps movement and held buttons");
	Check(!Sim::HasButton(repeat, Sim::BUTTON_JUMP) && repeat.yawDelta == 0.0f, "and drops presses and look");

	delete player;
	delete scene;
}

int main()
{
	// A Player binds a model when enabled, so a GL context has to exist first
	OpenTestWindow("veil-fixed-tick-tests", false, true);
	Client::InstallGamePresentation();

	std::printf("Fixed tick and PlayerCommand tests\n\n");

	TestClockRunsSixtyTicksASecond();
	TestFeelIsFrameRateIndependent();
	TestPressesFireOnce();
	TestFlashlightTogglesOncePerPress();
	TestLookFollowsCommands();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseTestWindow();
	return g_failures == 0 ? 0 : 1;
}
