/**
 * ServerWorld, ClientWorld and server-owned stations (MultiplayerSplitPlan.md
 * N2, N5, G7; rules P-5, P-6, P-8, S-3, S-4, S-7; DedicatedServerPlan T2.1,
 * T2.3).
 *
 *   T2.1       Single-player through Loopback ends in the same place as the
 *              simulation ticked directly with the same commands.
 *   Handshake  WorldMismatch, VersionMismatch and ServerFull are rejected; a
 *              second Hello is ignored; no Hello within 5 s closes.
 *   Commands   a flood doesn't move a player faster; over the poll budget a
 *              connection is closed; a 150 ms gap recovers.
 *   T2.3-lite  a second claim, a too-fast Complete, a Complete from a
 *              non-holder and a claim on a completed station are refused; the
 *              holder is released on disconnect and on a wipe; the Director
 *              skips held stations; too many requests a second disconnects.
 *
 * Several clients need several connections, so a small in-memory hub stands in
 * for the network here (Loopback joins exactly two ends).
 */

#include <raylib.h>

#include <Scene.h>
#include <SaveSystem.h>
#include <AI/Stalker.h>
#include <Client/ClientWorld.h>
#include <Client/LocalPlayer.h>
#include <Client/OnlineWorldSync.h>
#include <net/LoopbackTransport.h>
#include <net/Protocol.h>
#include <sim/FixedTick.h>
#include <sim/ServerWorld.h>
#include <World/FileWorldStore.h>

#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
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

// ---- An in-memory network with one server and many clients ----------------

struct Hub
{
	struct Item { Net::NetEventType type; Net::ConnectionId connection; std::vector<std::byte> bytes; };

	struct ServerEnd final : Net::INetTransport
	{
		Hub* hub = nullptr;
		std::deque<Item> inbox;
		std::vector<Item> delivered;

		bool Listen(std::uint16_t) override { return true; }
		Net::ConnectionId Connect(std::string_view) override { return Net::kInvalidConnection; }
		void Disconnect(Net::ConnectionId c) override { hub->CloseFromServer(c); }
		bool Send(Net::ConnectionId c, std::span<const std::byte> data, Net::Delivery) override { return hub->ToClient(c, data); }
		std::size_t Poll(std::span<Net::NetEvent> out) override { return Drain(inbox, delivered, out); }
		Net::NetStats Stats(Net::ConnectionId) const override { return {}; }
	};

	struct ClientEnd final : Net::INetTransport
	{
		Hub* hub = nullptr;
		Net::ConnectionId id = 0;
		bool open = false;
		std::deque<Item> inbox;
		std::vector<Item> delivered;

		bool Listen(std::uint16_t) override { return false; }
		Net::ConnectionId Connect(std::string_view) override { open = true; hub->Open(*this); return id; }
		void Disconnect(Net::ConnectionId) override { if (open) { open = false; hub->CloseFromClient(*this); } }
		bool Send(Net::ConnectionId, std::span<const std::byte> data, Net::Delivery) override
		{
			if (!open) { return false; }
			hub->server.inbox.push_back({ Net::NetEventType::Message, id, { data.begin(), data.end() } });
			return true;
		}
		std::size_t Poll(std::span<Net::NetEvent> out) override { return Drain(inbox, delivered, out); }
		Net::NetStats Stats(Net::ConnectionId) const override { return {}; }
	};

	static std::size_t Drain(std::deque<Item>& inbox, std::vector<Item>& delivered, std::span<Net::NetEvent> out)
	{
		delivered.clear();
		const std::size_t n = std::min(out.size(), inbox.size());
		for (std::size_t i = 0; i < n; ++i) { delivered.push_back(std::move(inbox.front())); inbox.pop_front(); }
		for (std::size_t i = 0; i < n; ++i)
		{
			out[i].type = delivered[i].type;
			out[i].connection = delivered[i].connection;
			out[i].payload = { delivered[i].bytes.data(), delivered[i].bytes.size() };
		}
		return n;
	}

	ServerEnd server;
	std::vector<std::unique_ptr<ClientEnd>> clients;

	Hub() { server.hub = this; }

	ClientEnd& NewClient()
	{
		auto end = std::make_unique<ClientEnd>();
		end->hub = this;
		end->id = static_cast<Net::ConnectionId>(clients.size() + 1);
		clients.push_back(std::move(end));
		return *clients.back();
	}

	void Open(ClientEnd& c)
	{
		server.inbox.push_back({ Net::NetEventType::Connected, c.id, {} });
		c.inbox.push_back({ Net::NetEventType::Connected, c.id, {} });
	}

	bool ToClient(Net::ConnectionId id, std::span<const std::byte> data)
	{
		for (auto& c : clients)
		{
			if (c->id == id && c->open) { c->inbox.push_back({ Net::NetEventType::Message, id, { data.begin(), data.end() } }); return true; }
		}
		return false;
	}

	void CloseFromServer(Net::ConnectionId id)
	{
		for (auto& c : clients)
		{
			if (c->id == id && c->open)
			{
				// Linger: what was sent before the close still arrives
				c->open = false;
				c->inbox.push_back({ Net::NetEventType::Disconnected, id, {} });
			}
		}
	}

	void CloseFromClient(ClientEnd& c)
	{
		server.inbox.push_back({ Net::NetEventType::Disconnected, c.id, {} });
	}
};

// ---- Scenes --------------------------------------------------------------

static SceneManagement::Scene* ChunkScene()
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	scene->SetSpawnPoint(Vector3(20, 2, 0));
	SaveSystem::LoadWorld("chunk_1", *scene);
	return scene;
}

// A floor and three stations in a row, everything unlocked, nothing timed
static SceneManagement::Scene* StationScene(float minimumSeconds = 0.0f)
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	scene->SetSpawnPoint(Vector3(0, 1, 0));
	for (int i = 0; i < 3; ++i)
	{
		InteractableObject station(INTERACT_MINIGAME, MINI_GAME_CRANE_ID, 0);
		station.name = "Station";
		station.minimumSeconds = minimumSeconds;
		station.rigidBody3D.Teleport(Vector3(static_cast<float>(i) * 4.0f, 1, 5));
		station.rigidBody3D.isStatic = true;
		scene->gameMap.SpawnInteractable(station);
	}
	scene->OnWorldLoaded();
	scene->ForEachPlayer([](int, Player& p) { p.artifactUnlocked = MINI_GAME_RO_SHAM_BOO_ID; p.rigidBody3D.SetGravity(0, 0, 0); });
	return scene;
}

static void Run(Sim::ServerWorld& server, Client::ClientWorld* client, SceneManagement::Scene* scene, int ticks, const Sim::PlayerCommand* command = nullptr, std::uint32_t* seq = nullptr)
{
	for (int i = 0; i < ticks; ++i)
	{
		if (client && command)
		{
			Sim::PlayerCommand c = *command;
			c.sequence = ++(*seq);
			client->SendCommand(c);
		}
		server.Tick(Sim::kTickDelta);
		if (client) { client->Poll(scene); }
	}
}

static float Planar(Vector3 a, Vector3 b) { return Vector2Distance(Vector2{ a.x, a.z }, Vector2{ b.x, b.z }); }

// ---- T2.1 ----------------------------------------------------------------

static void TestSinglePlayerThroughLoopback()
{
	std::printf("T2.1: single-player through Loopback matches the simulation ticked directly\n");

	// Directly
	SceneManagement::Scene* direct = ChunkScene();
	const Vector3 start = direct->PlayerInSlot(0)->getPosition();
	Sim::PlayerCommand walk = {};
	walk.moveY = 1;
	walk.yawDelta = 0.004f;
	for (int i = 0; i < 300; ++i)
	{
		Sim::PlayerCommand c = walk;
		c.sequence = static_cast<std::uint32_t>(i + 1);
		SceneManagement::Scene_tick(*direct, Sim::kTickDelta, std::span<const Sim::PlayerCommand>(&c, 1));
	}
	const Vector3 directEnd = direct->PlayerInSlot(0)->getPosition();

	// Through Loopback, with the server and client of a single-player session
	SceneManagement::Scene* scene = ChunkScene();
	auto [serverEnd, clientEnd] = Net::LoopbackTransport::CreatePair();
	serverEnd->Listen(0);
	World::FileWorldStore store;
	store.BeginOpen("chunk_1:0000000000000000");
	Sim::ServerConfig config;
	config.maxPlayers = 1;
	Sim::ServerWorld server(*serverEnd, config);
	server.SetWorldStore(&store);
	server.AttachScene(scene);
	Check(server.ApplyWorldState(), "World state applies once the store is Ready");

	Client::ClientWorld client(*clientEnd, false);
	client.Connect("loopback", scene->contentHash, "Kuma", scene->stations.Count());

	// The handshake takes a few ticks with nothing to send; then the same walk
	int handshake = 0;
	while (client.State() != Client::ConnectionState::Welcomed && handshake < 10)
	{
		server.Tick(Sim::kTickDelta);
		client.Poll(scene);
		++handshake;
	}
	Check(client.State() == Client::ConnectionState::Welcomed && client.Slot() == 0, "welcomed as Slot 0 (" + std::to_string(handshake) + " ticks)");

	// Undo the handshake's ticks so both runs start equal
	scene->PlayerInSlot(0)->rigidBody3D.Teleport(start);
	scene->PlayerInSlot(0)->rigidBody3D.SetVelocity(Vector3Zero());
	std::uint32_t seq = 0;
	Run(server, &client, scene, 300, &walk, &seq);

	const Vector3 loopEnd = scene->PlayerInSlot(0)->getPosition();
	Check(Planar(loopEnd, directEnd) < 0.05f, "the walk ends in the same place (off by " + std::to_string(Planar(loopEnd, directEnd)) + ")");
	Check(Planar(loopEnd, start) > 5.0f, "and it went somewhere");
	Check(client.AckedSequence() >= 298, "the server acks the commands it applied");
	Check(client.NewestServerTick() > 0, "Snapshots arrive");

	SceneManagement::Scene_delete(direct);
	SceneManagement::Scene_delete(scene);
}

// ---- Handshake -----------------------------------------------------------

static void TestHandshakeRejects()
{
	std::printf("N5: the handshake refuses another World save, another version and a full server\n");

	SceneManagement::Scene* scene = StationScene();
	Hub hub;
	Sim::ServerConfig config;
	config.maxPlayers = 2;
	Sim::ServerWorld server(hub.server, config);
	server.AttachScene(scene);
	server.ApplyWorldState();

	// WorldMismatch
	Hub::ClientEnd& wrongWorld = hub.NewClient();
	Client::ClientWorld wrong(wrongWorld, true);
	wrong.Connect("x", scene->contentHash ^ 1, "Wrong", scene->stations.Count());
	for (int i = 0; i < 5; ++i) { server.Tick(Sim::kTickDelta); wrong.Poll(scene); }
	Check(wrong.State() == Client::ConnectionState::Rejected && wrong.RejectedFor() == Net::RejectReason::WorldMismatch, "a different World save is refused: WorldMismatch");
	Check(server.WelcomedCount() == 0, "and takes no Slot");

	// VersionMismatch: a Hello from protocol version 2
	Hub::ClientEnd& oldVersion = hub.NewClient();
	oldVersion.Connect("x");
	server.Tick(Sim::kTickDelta);
	Net::Encoder encoder;
	auto hello = encoder.Encode(Net::HelloMsg{ scene->contentHash, "Future" }, server.WireLimits());
	std::vector<std::byte> v2(hello.begin(), hello.end());
	v2[1] = std::byte{ 2 };
	oldVersion.Send(oldVersion.id, v2, Net::Delivery::Reliable);
	server.Tick(Sim::kTickDelta);
	Net::Decoder decoder;
	bool sawReject = false;
	std::array<Net::NetEvent, 16> events;
	const std::size_t n = oldVersion.Poll(events);
	for (std::size_t i = 0; i < n; ++i)
	{
		Net::Message m;
		if (events[i].type == Net::NetEventType::Message && decoder.Decode(events[i].payload, server.WireLimits(), m) == Net::DecodeError::None)
		{
			if (const auto* r = std::get_if<Net::RejectMsg>(&m)) { sawReject = r->reason == Net::RejectReason::VersionMismatch; }
		}
	}
	Check(sawReject && !oldVersion.open, "another protocol version is refused with a readable Reject(VersionMismatch), then closed");

	// Two fit, the third doesn't
	Hub::ClientEnd& a = hub.NewClient();
	Hub::ClientEnd& b = hub.NewClient();
	Hub::ClientEnd& c = hub.NewClient();
	Client::ClientWorld ca(a, true), cb(b, true), cc(c, true);
	ca.Connect("x", scene->contentHash, "A", scene->stations.Count());
	cb.Connect("x", scene->contentHash, "B", scene->stations.Count());
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); ca.Poll(scene); cb.Poll(scene); }
	cc.Connect("x", scene->contentHash, "C", scene->stations.Count());
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); cc.Poll(scene); }
	Check(ca.State() == Client::ConnectionState::Welcomed && cb.State() == Client::ConnectionState::Welcomed && ca.Slot() != cb.Slot(), "two clients get two Slots");
	Check(cc.State() == Client::ConnectionState::Rejected && cc.RejectedFor() == Net::RejectReason::ServerFull, "the third is refused: ServerFull");
	Check(scene->PlayerCount() == 2, "the scene has a player per welcomed Slot");

	// A second Hello is ignored
	ca.Connect("x", scene->contentHash, "A again", scene->stations.Count()); // reconnect is a new Connected on the hub
	server.Tick(Sim::kTickDelta);
	Check(server.WelcomedCount() == 2, "a second Hello takes no second Slot");

	// The Slot frees on disconnect
	b.Disconnect(b.id);
	server.Tick(Sim::kTickDelta);
	Check(server.WelcomedCount() <= 1 + 1 && scene->PlayerInSlot(cb.Slot()) == nullptr, "a disconnect frees the Slot and its player");

	Client::SetLocalSlot(0);
	SceneManagement::Scene_delete(scene);
}

static void TestHelloTimeout()
{
	std::printf("P-5: a connection with no Hello within 5 s is closed\n");
	SceneManagement::Scene* scene = StationScene();
	Hub hub;
	Sim::ServerWorld server(hub.server);
	server.AttachScene(scene);
	Hub::ClientEnd& silent = hub.NewClient();
	silent.Connect("x");
	for (int i = 0; i < 5 * 60 - 5; ++i) { server.Tick(Sim::kTickDelta); }
	Check(silent.open, "still open just under 5 s");
	for (int i = 0; i < 10; ++i) { server.Tick(Sim::kTickDelta); }
	Check(!silent.open && server.LastCloseReason().find("Hello") != std::string::npos, "closed at 5 s");
	SceneManagement::Scene_delete(scene);
}

// ---- Commands ------------------------------------------------------------

static float WalkWithMessagesPerTick(int perTick, bool* stillOpen)
{
	SceneManagement::Scene* scene = StationScene();
	Hub hub;
	Sim::ServerWorld server(hub.server);
	server.AttachScene(scene);
	Hub::ClientEnd& end = hub.NewClient();
	Client::ClientWorld client(end, true);
	client.Connect("x", scene->contentHash, "Flood", scene->stations.Count());
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); client.Poll(scene); }

	Player* player = scene->PlayerInSlot(client.Slot());
	const Vector3 start = player ? player->getPosition() : Vector3Zero();
	std::uint32_t seq = 0;
	for (int t = 0; t < 60; ++t)
	{
		for (int m = 0; m < perTick; ++m)
		{
			Sim::PlayerCommand c = {};
			c.moveY = 1;
			c.sequence = ++seq;
			client.SendCommand(c);
		}
		server.Tick(Sim::kTickDelta);
		client.Poll(scene);
	}
	player = scene->PlayerInSlot(client.Slot());
	const float distance = player ? Planar(start, player->getPosition()) : -1.0f;
	if (stillOpen) { *stillOpen = end.open; }
	Client::SetLocalSlot(0);
	SceneManagement::Scene_delete(scene);
	return distance;
}

static void TestCommandFloodAndBudget()
{
	std::printf("P-6, P-7: one command a tick, whatever arrives; a flood over budget closes\n");
	bool open = false;
	const float normal = WalkWithMessagesPerTick(1, &open);
	const float flooded = WalkWithMessagesPerTick(10, &open);
	Check(normal > 1.0f, "a normal client walks (" + std::to_string(normal) + ")");
	Check(std::fabs(flooded - normal) < 0.05f, "ten times the commands move the player no further (" + std::to_string(flooded) + ")");
	Check(open, "ten messages a tick is within the budget");
	WalkWithMessagesPerTick(40, &open);
	Check(!open, "forty a tick is over the budget: closed");
}

static void TestGapRecovers()
{
	std::printf("P-6: after a 150 ms gap, new commands are applied again\n");
	SceneManagement::Scene* scene = StationScene();
	Hub hub;
	Sim::ServerWorld server(hub.server);
	server.AttachScene(scene);
	Hub::ClientEnd& end = hub.NewClient();
	Client::ClientWorld client(end, true);
	client.Connect("x", scene->contentHash, "Gap", scene->stations.Count());
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); client.Poll(scene); }

	std::uint32_t seq = 0;
	Sim::PlayerCommand walk = {};
	walk.moveY = 1;
	Run(server, &client, scene, 30, &walk, &seq);
	const std::uint32_t before = client.AckedSequence();

	// 9 ticks (150 ms) of commands lost on the way
	for (int i = 0; i < 9; ++i) { ++seq; server.Tick(Sim::kTickDelta); client.Poll(scene); }
	Run(server, &client, scene, 30, &walk, &seq);
	Check(client.AckedSequence() > before + 20, "commands after the gap are acked (" + std::to_string(before) + " -> " + std::to_string(client.AckedSequence()) + ")");
	Client::SetLocalSlot(0);
	SceneManagement::Scene_delete(scene);
}

// ---- Stations ------------------------------------------------------------

static int Count(const std::vector<Sim::GameEvent>& events, Sim::StationEventKind kind)
{
	int n = 0;
	for (const auto& e : events) { if (e.type == Sim::GameEventType::StationEvent && e.stationEvent == kind) { ++n; } }
	return n;
}

static void TestStationRules()
{
	std::printf("T2.3-lite: the server's station rules (S-3, S-4)\n");
	SceneManagement::Scene* scene = StationScene(2.0f);
	scene->AddPlayer(1)->artifactUnlocked = MINI_GAME_RO_SHAM_BOO_ID;
	scene->PlayerInSlot(1)->rigidBody3D.Teleport(Vector3(0, 1, 0));
	scene->TakeEvents();

	Check(scene->ClaimStation(0, 0), "Slot 0 claims station 0");
	Check(Count(scene->TakeEvents(), Sim::StationEventKind::Granted) == 1, "Granted");
	Check(scene->gameMap.FindRunningStation() != nullptr, "the station is in use");

	Check(!scene->ClaimStation(0, 1), "Slot 1 can't claim a held station");
	Check(Count(scene->TakeEvents(), Sim::StationEventKind::Denied) == 1, "Denied");
	Check(!scene->ClaimStation(1, 0), "Slot 0 can't hold two stations");
	scene->TakeEvents();

	Check(!scene->CompleteStation(0, 0), "a Complete before the minimum seconds is refused");
	Check(!scene->CompleteStation(0, 1), "a Complete from a non-holder is refused");
	scene->TakeEvents();

	for (int i = 0; i < static_cast<int>(2.0f * Sim::kTickRate) + 1; ++i) { SceneManagement::Scene_tick(*scene, Sim::kTickDelta); }
	scene->TakeEvents();
	Check(scene->CompleteStation(0, 0), "after the minimum seconds the holder completes it");
	Check(scene->stations.At(0).Completed() && !scene->stations.At(0).InUse(), "completed and free");
	scene->TakeEvents();

	Check(!scene->ClaimStation(0, 1), "a completed station can't be claimed");
	scene->TakeEvents();

	// Released on a wipe
	Check(scene->ClaimStation(1, 1), "Slot 1 claims station 1");
	scene->FullWipe();
	Check(!scene->stations.At(1).InUse() && !scene->stations.At(0).Completed(), "a Full wipe frees every holder and undoes completion");

	// Too far away
	scene->PlayerInSlot(1)->rigidBody3D.Teleport(Vector3(100, 1, 100));
	Check(!scene->ClaimStation(2, 1), "a player too far away can't claim");

	// The unlock gate, on the server
	scene->PlayerInSlot(1)->rigidBody3D.Teleport(Vector3(8, 1, 3));
	scene->PlayerInSlot(1)->artifactUnlocked = -1;
	Check(!scene->ClaimStation(2, 1), "a game not unlocked can't be claimed");

	SceneManagement::Scene_delete(scene);
}

static void TestDirectorSkipsHeldStations()
{
	std::printf("S-7: the Director never hints at a held station\n");
	SceneManagement::Scene* scene = StationScene();
	auto stalker = std::make_unique<Stalker>();
	stalker->id = 5000;
	stalker->rigidBody3D.Teleport(Vector3(0, 0, -20));
	stalker->rigidBody3D.isStatic = true;
	stalker->onEnable();
	scene->gameMap.RestoreEntity(std::move(stalker));

	// The farthest station from the stalker is held
	scene->ClaimStation(2, 0);
	for (int i = 0; i < 60; ++i) { scene->soundField.Update(0.5f); }
	scene->director.Update(scene, 30.0f);
	const auto& log = scene->director.Log();
	const InteractableObject* held = scene->gameMap.FindInteractable(scene->stations.At(2).id);
	Check(!log.empty() && held && Vector3Distance(log.back().region, held->getPosition()) > 0.5f, "the hint goes to an unheld station");
	SceneManagement::Scene_delete(scene);
}

static void TestDisconnectReleasesAndRequestRate()
{
	std::printf("S-4: a disconnect releases the holder; too many requests a second disconnects\n");
	SceneManagement::Scene* scene = StationScene();
	Hub hub;
	Sim::ServerWorld server(hub.server);
	server.AttachScene(scene);

	Hub::ClientEnd& end = hub.NewClient();
	Client::ClientWorld client(end, true);
	client.Connect("x", scene->contentHash, "Holder", scene->stations.Count());
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); client.Poll(scene); }
	const int slot = client.Slot();
	scene->PlayerInSlot(slot)->artifactUnlocked = MINI_GAME_RO_SHAM_BOO_ID;
	scene->PlayerInSlot(slot)->rigidBody3D.Teleport(Vector3(0, 1, 3));

	scene->stationRequests.push_back({ Sim::StationAction::Claim, 1 });
	client.SendStationRequests(*scene);
	server.Tick(Sim::kTickDelta);
	client.Poll(scene);
	Check(scene->stations.At(1).holder == slot, "a claim over the wire is granted to the connection's Slot");

	end.Disconnect(end.id);
	server.Tick(Sim::kTickDelta);
	Check(!scene->stations.At(1).InUse(), "the disconnect released it");

	Hub::ClientEnd& spammer = hub.NewClient();
	Client::ClientWorld spam(spammer, true);
	spam.Connect("x", scene->contentHash, "Spam", scene->stations.Count());
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); spam.Poll(scene); }
	for (int i = 0; i < 6; ++i) { scene->stationRequests.push_back({ Sim::StationAction::Release, 0 }); }
	spam.SendStationRequests(*scene);
	server.Tick(Sim::kTickDelta);
	Check(!spammer.open, "six requests in a second disconnects");
	Client::SetLocalSlot(0);
	SceneManagement::Scene_delete(scene);
}

static void TestNewEpochEndsTheRound()
{
	std::printf("P-8: a catch ends the round for the client at once\n");
	SceneManagement::Scene* scene = StationScene();
	auto [serverEnd, clientEnd] = Net::LoopbackTransport::CreatePair();
	serverEnd->Listen(0);
	Sim::ServerWorld server(*serverEnd);
	server.AttachScene(scene);
	Client::ClientWorld client(*clientEnd, false);
	client.Connect("loopback", scene->contentHash, "P", scene->stations.Count());
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); client.Poll(scene); }

	// Claim through Interact-equivalent and let the grant open the MiniGame
	scene->ClaimStation(0, 0);
	server.Tick(Sim::kTickDelta);
	client.Poll(scene);
	Check(scene->miniGameSession.IsRunning() && scene->overlays.IsOpen(Overlay::MiniGame), "the grant opens the MiniGame on the client");

	scene->RaiseCatch(scene->PlayerInSlot(0)->id);
	server.Tick(Sim::kTickDelta); // ResolveRound runs inside the tick
	client.Poll(scene);
	Check(!scene->miniGameSession.IsRunning(), "the round's end closes the MiniGame");
	Check(scene->overlays.IsOpen(Overlay::Jumpscare), "and shows the Jumpscare");
	Check(client.RoundEpoch() == scene->roundEpoch, "the client is on the new epoch");
	Check(!scene->stations.At(0).InUse(), "and the station is free again");
	SceneManagement::Scene_delete(scene);
}

static void TestOnlineWorldSync()
{
	std::printf("Online world sync: the joined server's World state lands in a temp file\n");
	const std::string key = "synctest:0123456789abcdef";
	Client::DiscardOnlineWorldState(key);

	SceneManagement::Scene* scene = StationScene();
	Hub hub;
	Sim::ServerWorld server(hub.server);
	server.AttachScene(scene);

	Hub::ClientEnd& end = hub.NewClient();
	Client::ClientWorld client(end, true);
	client.Connect("x", scene->contentHash, "Syncer", scene->stations.Count());

	// Not Welcomed, no Snapshot yet: nothing is written
	Check(!Client::SyncWorldFromServer(client, scene->stations, key), "no file before the client is Welcomed");
	Check(!std::filesystem::exists(Client::OnlineWorldFile(key)), "and none on disk");

	for (int i = 0; i < 6; ++i) { server.Tick(Sim::kTickDelta); client.Poll(scene); }
	Check(client.State() == Client::ConnectionState::Welcomed && client.NewestServerTick() > 0, "Welcomed with a Snapshot received");

	// Nothing completed through play yet: an empty list, still a valid file
	std::filesystem::path written;
	Check(Client::SyncWorldFromServer(client, scene->stations, key, &written), "sync writes once there is a Snapshot");
	Check(written == Client::OnlineWorldFile(key) && std::filesystem::exists(written), "to the key's temp file");
	World::WorldState state;
	Check(Client::ReadOnlineWorldState(written, key, state) && state.completedStations.empty(), "it reads back empty");

	// A completion on the server shows up in the next sync
	scene->PlayerInSlot(client.Slot())->artifactUnlocked = MINI_GAME_RO_SHAM_BOO_ID;
	scene->PlayerInSlot(client.Slot())->rigidBody3D.Teleport(Vector3(4, 1, 3));
	Check(scene->ClaimStation(1, client.Slot()), "Slot claims station 1");
	Check(scene->CompleteStation(1, client.Slot()), "and completes it");
	scene->TakeEvents();
	const std::uint64_t completedId = scene->stations.At(1).id;
	for (int i = 0; i < 4; ++i) { server.Tick(Sim::kTickDelta); client.Poll(scene); }

	Check(Client::SyncWorldFromServer(client, scene->stations, key), "sync again");
	World::WorldState after;
	Check(Client::ReadOnlineWorldState(written, key, after), "reads back");
	Check(after.completedStations.size() == 1 && after.completedStations[0] == completedId, "holding exactly the completed station's id");
	Check(!std::filesystem::exists(written.string() + ".tmp"), "no partial file is left behind");

	// A file for another world, or a damaged one, is refused
	World::WorldState foreign;
	Check(!Client::ReadOnlineWorldState(written, "other:0000000000000000", foreign), "another world key is refused");
	{
		std::ofstream bad(written, std::ios::binary | std::ios::trunc);
		bad << "{ not json";
	}
	Check(!Client::ReadOnlineWorldState(written, key, foreign), "a damaged file is refused");

	Client::DiscardOnlineWorldState(key);
	Check(!std::filesystem::exists(written), "Discard removes the file");
	Client::DiscardOnlineWorldState(key); // a missing file is fine

	Client::SetLocalSlot(0);
	SceneManagement::Scene_delete(scene);
}

int main()
{
	OpenTestWindow("veil-server-client-tests", true, true);
	Client::InstallGamePresentation();

	std::printf("Server and client tests\n\n");

	TestSinglePlayerThroughLoopback();
	TestHandshakeRejects();
	TestHelloTimeout();
	TestCommandFloodAndBudget();
	TestGapRecovers();
	TestStationRules();
	TestDirectorSkipsHeldStations();
	TestDisconnectReleasesAndRequestRate();
	TestNewEpochEndsTheRound();
	TestOnlineWorldSync();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseTestWindow();
	return g_failures == 0 ? 0 : 1;
}
