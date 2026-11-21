/**
 * GnsTransport over real sockets on 127.0.0.1 (MultiplayerSplitPlan.md N4, N5;
 * rules R6, P-5, P-7).
 *
 *   N4  Two ends connect; Stats() is filled; a message over 2048 B closes the
 *       connection (RecvMaxMessageSize); only GNS calls are behind the header.
 *   N5  Over GNS: Welcome; ServerFull and WorldMismatch are rejected and the
 *       Reject still arrives (close with linger); commands are acked; Snapshots
 *       arrive; a client notices the server going away.
 *
 * Uses ports 27777 and up, so a game hosting on 7777 isn't disturbed.
 */

#include <raylib.h>

#include <Scene.h>
#include <Client/ClientWorld.h>
#include <Client/LocalPlayer.h>
#include <net/gns/GnsTransport.h>
#include <net/Protocol.h>
#include <sim/FixedTick.h>
#include <sim/ServerWorld.h>

#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
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

// Runs `step` every millisecond until `done` or the time runs out
static bool Until(const std::function<void()>& step, const std::function<bool()>& done, double seconds = 5.0)
{
	const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
	while (std::chrono::steady_clock::now() < end)
	{
		step();
		if (done()) { return true; }
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return done();
}

static SceneManagement::Scene* Room()
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	InteractableObject station(INTERACT_MINIGAME, MINI_GAME_CRANE_ID, 0);
	station.rigidBody3D.Teleport(Vector3(0, 1, 5));
	scene->gameMap.SpawnInteractable(station);
	scene->OnWorldLoaded();
	return scene;
}

static void TestRawConnectAndOversize()
{
	std::printf("N4: two GNS ends connect; Stats is filled; an oversize message closes\n");
	auto server = Net::Gns::CreateTransport();
	auto client = Net::Gns::CreateTransport();
	Check(server->Listen(27777), "the server listens");

	const Net::ConnectionId toServer = client->Connect("127.0.0.1:27777");
	Check(toServer != Net::kInvalidConnection, "the client starts connecting");

	std::array<Net::NetEvent, 32> events;
	Net::ConnectionId toClient = Net::kInvalidConnection;
	bool clientConnected = false;
	const bool connected = Until([&] {
		for (std::size_t i = 0, n = server->Poll(events); i < n; ++i) { if (events[i].type == Net::NetEventType::Connected) { toClient = events[i].connection; } }
		for (std::size_t i = 0, n = client->Poll(events); i < n; ++i) { if (events[i].type == Net::NetEventType::Connected) { clientConnected = true; } }
	}, [&] { return toClient != Net::kInvalidConnection && clientConnected; });
	Check(connected, "both ends see Connected");

	const char hello[] = "hello over gns";
	client->Send(toServer, std::as_bytes(std::span(hello)), Net::Delivery::Reliable);
	std::string got;
	Until([&] {
		for (std::size_t i = 0, n = server->Poll(events); i < n; ++i)
		{
			if (events[i].type == Net::NetEventType::Message) { got.assign(reinterpret_cast<const char*>(events[i].payload.data()), events[i].payload.size()); }
		}
		client->Poll(events);
	}, [&] { return !got.empty(); });
	Check(got == std::string(hello, sizeof(hello)), "a reliable message arrives intact");

	// Traffic both ways for a moment so the stats have something to measure
	Until([&] {
		server->Send(toClient, std::as_bytes(std::span(hello)), Net::Delivery::Unreliable);
		client->Send(toServer, std::as_bytes(std::span(hello)), Net::Delivery::Unreliable);
		server->Poll(events);
		client->Poll(events);
	}, [] { return false; }, 6.0); // GNS updates its rates on a multi-second interval
	const Net::NetStats stats = client->Stats(toServer);
	std::printf("  client stats: ping %.1f ms, loss %.3f, in %.0f B/s, out %.0f B/s\n", stats.pingMs, stats.packetLoss, stats.bytesInPerSec, stats.bytesOutPerSec);
	Check(stats.bytesOutPerSec > 0.0f && stats.bytesInPerSec > 0.0f, "Stats() reports bytes in and out (" + std::to_string(stats.bytesInPerSec) + " B/s in)");

	// Over RecvMaxMessageSize: the receiver closes the connection (P-7)
	std::vector<std::byte> huge(Net::kMaxMessage + 512, std::byte{ 0x55 });
	client->Send(toServer, huge, Net::Delivery::Reliable);
	bool serverSawClose = false, clientSawClose = false;
	Until([&] {
		for (std::size_t i = 0, n = server->Poll(events); i < n; ++i) { if (events[i].type == Net::NetEventType::Disconnected) { serverSawClose = true; } }
		for (std::size_t i = 0, n = client->Poll(events); i < n; ++i) { if (events[i].type == Net::NetEventType::Disconnected) { clientSawClose = true; } }
	}, [&] { return serverSawClose && clientSawClose; }, 8.0);
	Check(serverSawClose && clientSawClose, "a message over 2048 B closes the connection on both ends");
}

struct Party
{
	std::unique_ptr<Net::INetTransport> transport;
	std::unique_ptr<Client::ClientWorld> world;
};

static void TestHandshakeOverGns()
{
	std::printf("N5: the handshake, commands and Snapshots over GNS\n");
	SceneManagement::Scene* scene = Room();
	auto serverTransport = Net::Gns::CreateTransport();
	Check(serverTransport->Listen(27778), "the server listens");
	Sim::ServerConfig config;
	config.maxPlayers = 2;
	Sim::ServerWorld server(*serverTransport, config);
	server.AttachScene(scene);
	server.ApplyWorldState();

	// Each client draws its own copy, as a joined client does
	std::vector<SceneManagement::Scene*> copies;
	std::vector<Party> parties;
	parties.reserve(8); // references to earlier parties stay valid as more join
	const auto join = [&](std::uint64_t hash, const char* name) -> Party& {
		Party party;
		party.transport = Net::Gns::CreateTransport();
		party.world = std::make_unique<Client::ClientWorld>(*party.transport, true);
		copies.push_back(Room());
		party.world->Connect("127.0.0.1:27778", hash, name, scene->stations.Count());
		parties.push_back(std::move(party));
		return parties.back();
	};
	const auto step = [&] {
		server.Tick(Sim::kTickDelta);
		for (std::size_t i = 0; i < parties.size(); ++i) { parties[i].world->Poll(copies[i]); }
	};

	Party& a = join(scene->contentHash, "Alpha");
	Party& b = join(scene->contentHash, "Bravo");
	Check(Until(step, [&] { return a.world->State() == Client::ConnectionState::Welcomed && b.world->State() == Client::ConnectionState::Welcomed; }),
		"two clients are welcomed");
	Check(a.world->Slot() != b.world->Slot() && scene->PlayerCount() == 2, "into two Slots, each with a player");

	Party& c = join(scene->contentHash, "Charlie");
	Check(Until(step, [&] { return c.world->State() == Client::ConnectionState::Rejected; }), "a third is refused");
	Check(c.world->RejectedFor() == Net::RejectReason::ServerFull, "with ServerFull, which arrived before the close (linger)");

	Party& d = join(scene->contentHash ^ 0xff, "Delta");
	Check(Until(step, [&] { return d.world->State() == Client::ConnectionState::Rejected; }), "a client with another World save is refused");
	Check(d.world->RejectedFor() == Net::RejectReason::WorldMismatch, "with WorldMismatch");

	// Commands up, Snapshots down
	std::uint32_t seq = 0;
	Check(Until([&] {
		Sim::PlayerCommand command = {};
		command.sequence = ++seq;
		command.moveY = 1;
		a.world->SendCommand(command);
		step();
	}, [&] { return a.world->AckedSequence() > 20 && a.world->NewestServerTick() > 0; }), "commands are acked and Snapshots arrive");

	a.world->ApplySnapshots(*copies[0], 0.0);
	Check(copies[0]->PlayerInSlot(b.world->Slot()) != nullptr, "the client's own copy shows the other player");

	// The server going away is noticed
	server.DisconnectAll();
	Check(Until([&] { for (std::size_t i = 0; i < parties.size(); ++i) { parties[i].world->Poll(copies[i]); } },
		[&] { return a.world->State() == Client::ConnectionState::Lost && b.world->State() == Client::ConnectionState::Lost; }),
		"clients see the server close");

	for (auto* copy : copies) { SceneManagement::Scene_delete(copy); }
	parties.clear();
	Client::SetLocalSlot(0);
	SceneManagement::Scene_delete(scene);
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	OpenTestWindow("veil-gns-tests", true, true);
	Client::InstallGamePresentation();

	std::printf("GNS transport tests\n\n");

	std::string error;
	Check(Net::Gns::Init(error), "GameNetworkingSockets starts (" + error + ")");
	if (Net::Gns::IsRunning())
	{
		TestRawConnectAndOversize();
		TestHandshakeOverGns();
		Net::Gns::Kill();
	}
	Check(!Net::Gns::IsRunning(), "and stops");

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseTestWindow();
	return g_failures == 0 ? 0 : 1;
}
