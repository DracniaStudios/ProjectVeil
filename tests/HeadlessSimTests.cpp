/**
 * The headless simulation test (work order A-11; decision D3, DedicatedServerPlan T1.1).
 *
 * Links veil_sim and nothing else: no window, no GL context, no audio device, no
 * AssetManager, no LightingSystem and no presentation installed. If a simulation
 * file reaches for any of those, this target does not link.
 *
 *   T1.1       10,000 ticks of chunk_1 with commands only, as RenderOffTickTests does.
 *   No GPU     nothing the World save built holds a mesh.
 *   Server     a ServerWorld session over Loopback: Hello, Welcome, commands,
 *              snapshots.
 *   Digest     the tick digest of chunk_1 with no presentation installed: non-zero
 *              and repeatable. It is NOT the windowed golden: the client's
 *              BindVisuals refits mesh colliders to the bound model, headless they
 *              keep what the World save recorded (decision D7). A-12 compares them.
 */

#include <raylib.h>   // types and inline math only: no raylib function is called here

#include "TickDigest.h"

#include <Scene.h>
#include <SaveSystem.h>
#include <AI/Stalker.h>
#include <net/LoopbackTransport.h>
#include <net/Protocol.h>
#include <sim/FixedTick.h>
#include <sim/PlayerCommand.h>
#include <sim/ServerWorld.h>
#include <sim/Worlds.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <variant>

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

static bool Finite(const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

static Stalker* FindStalker(SceneManagement::Scene& scene)
{
	Stalker* found = nullptr;
	scene.gameMap.ForEachStalker([&](Stalker& stalker) { found = &stalker; return false; });
	return found;
}

static SceneManagement::Scene* TutorialScene()
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	Check(Sim::LoadTutorialWorld(*scene), "the Tutorial's World save loads");
	return scene;
}

// T1.1 and the no-GPU check, on the same scene
static void TestTenThousandTicksHeadless()
{
	std::printf("T1.1: 10,000 ticks of chunk_1 with no window\n");
	SceneManagement::Scene* scene = TutorialScene();
	Check(scene->contentHash != 0, "the World save's content hash is set");
	std::printf("  content hash (headless): 0x%016llx\n", static_cast<unsigned long long>(scene->contentHash));

	Stalker* stalker = FindStalker(*scene);
	Check(stalker != nullptr, "the World save has a stalker");
	if (stalker == nullptr || scene->PlayerInSlot(0) == nullptr) { SceneManagement::Scene_delete(scene); return; }

	// No GPU: nothing was bound
	int objects = 0;
	int withMesh = 0;
	const auto count = [&](GameObject& o) { ++objects; if (o.model.meshCount != 0) { ++withMesh; } };
	scene->gameMap.ForEachGameObject(count);
	scene->gameMap.ForEachEntity(count);
	scene->gameMap.ForEachInteractable(count);
	Check(objects > 0, "the World save made objects");
	Check(withMesh == 0, "no object holds a mesh (" + std::to_string(withMesh) + " of " + std::to_string(objects) + ")");
	Check(scene->PlayerInSlot(0)->model.meshCount == 0, "the Slot 0 player holds no mesh");

	const Vector3 playerStart = scene->PlayerInSlot(0)->getPosition();
	const float soundStart = scene->soundField.Now();
	const float directorStart = scene->director.Now();
	float stalkerTravel = 0.0f;
	Vector3 stalkerLast = stalker->getPosition();

	const auto started = std::chrono::steady_clock::now();
	constexpr int kTicks = 10000;
	for (int tick = 0; tick < kTicks; ++tick)
	{
		// Walk forward, turning slowly, crouched so the player makes no noise
		Sim::PlayerCommand command = {};
		command.sequence = static_cast<std::uint32_t>(tick + 1);
		command.moveY = 1;
		command.buttons = Sim::BUTTON_CROUCH;
		command.yawDelta = 0.002f;
		SceneManagement::Scene_tick(*scene, Sim::kTickDelta, std::span<const Sim::PlayerCommand>(&command, 1));

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

// A ServerWorld session over Loopback, on a fresh scene
static void TestServerOverLoopback()
{
	std::printf("Server: a session over Loopback with no window\n");
	SceneManagement::Scene* scene = TutorialScene();

	auto [serverEnd, clientEnd] = Net::LoopbackTransport::CreatePair();
	serverEnd->Listen(0);
	Sim::ServerWorld server(*serverEnd);
	server.AttachScene(scene);
	Check(server.ApplyWorldState(), "World state applies");

	Net::Encoder encoder;
	Net::Decoder decoder;
	bool connected = false;
	bool welcomed = false;
	int welcomeSlot = -1;
	int snapshots = 0;
	const Net::ConnectionId connection = clientEnd->Connect("loopback");
	Check(connection != Net::kInvalidConnection, "the client end connects");

	// Everything the client end receives: Connected sends Hello, as ClientWorld::Poll does
	const auto pollClient = [&]()
	{
		std::array<Net::NetEvent, 64> events;
		for (;;)
		{
			const std::size_t n = clientEnd->Poll(events);
			for (std::size_t i = 0; i < n; ++i)
			{
				const Net::NetEvent& event = events[i];
				if (event.type == Net::NetEventType::Connected)
				{
					connected = true;
					Net::HelloMsg hello;
					hello.worldHash = scene->contentHash;
					hello.name = "Headless";
					clientEnd->Send(connection, encoder.Encode(hello, server.WireLimits()), Net::Delivery::Reliable);
				}
				else if (event.type == Net::NetEventType::Message)
				{
					Net::Message message;
					if (decoder.Decode(event.payload, server.WireLimits(), message) != Net::DecodeError::None) { continue; }
					if (const auto* welcome = std::get_if<Net::WelcomeMsg>(&message)) { welcomed = true; welcomeSlot = welcome->slot; }
					else if (std::holds_alternative<Net::SnapshotMsg>(message)) { ++snapshots; }
				}
			}
			if (n < events.size()) { break; }
		}
	};

	pollClient();
	for (int i = 0; i < 20 && !welcomed; ++i) { server.Tick(Sim::kTickDelta); pollClient(); }
	Check(connected, "the client end sees Connected");
	Check(welcomed && welcomeSlot == 0, "a Welcome with slot 0 decodes");
	if (!welcomed || scene->PlayerInSlot(0) == nullptr) { SceneManagement::Scene_delete(scene); return; }

	const Vector3 start = scene->PlayerInSlot(0)->getPosition();
	for (int tick = 0; tick < 300; ++tick)
	{
		Net::CommandsMsg commands;
		commands.count = 1;
		commands.commands[0].sequence = static_cast<std::uint32_t>(tick + 1);
		commands.commands[0].moveY = 1;
		clientEnd->Send(connection, encoder.Encode(commands, server.WireLimits()), Net::Delivery::Unreliable);
		server.Tick(Sim::kTickDelta);
		pollClient();
	}

	Check(snapshots >= 1, "at least one Snapshot decoded (" + std::to_string(snapshots) + ")");
	Check(server.ServerTick() > 300, "the server's tick passed 300 (" + std::to_string(server.ServerTick()) + ")");
	const float moved = Vector3Distance(start, scene->PlayerInSlot(0)->getPosition());
	Check(moved > 1.0f, "the Slot 0 player moved under its commands (" + std::to_string(moved) + ")");

	SceneManagement::Scene_delete(scene);
}

// The digest with nothing installed: recorded, non-zero, repeatable
static void TestHeadlessDigest()
{
	std::printf("Digest: chunk_1 with no presentation installed\n");
	constexpr int kTicks = 3000;
	const std::uint64_t first = TickDigest::RunChunk1(kTicks);
	const std::uint64_t second = TickDigest::RunChunk1(kTicks);
	std::printf("  [TickDigest] chunk_1, %d ticks, headless: 0x%016llx (second run 0x%016llx)\n", kTicks,
		static_cast<unsigned long long>(first), static_cast<unsigned long long>(second));
	Check(first != 0, "the headless digest is non-zero");
	Check(first == second, "two headless runs in one process give the same digest");
}

int main()
{
	std::printf("Headless simulation tests (no window, no presentation)\n\n");

	TestTenThousandTicksHeadless();
	TestServerOverLoopback();
	TestHeadlessDigest();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
