#include "Session.h"

#include <Scene.h>
#include <SceneManager.h>
#include <Client/ClientWorld.h>
#include <Client/LocalPlayer.h>
#include <World/FileWorldStore.h>
#include <World/WorldKey.h>
#include <net/LoopbackTransport.h>
#include <sim/FixedTick.h>
#include <sim/ServerWorld.h>

#include <iostream>

namespace Client {

	const char* SessionModeName(SessionMode mode)
	{
		switch (mode)
		{
		case SessionMode::Offline: return "Offline";
		case SessionMode::Host:    return "Host";
		case SessionMode::Join:    return "Join";
		}
		return "Unknown";
	}

	Session& Session::getInstance()
	{
		static Session instance;
		return instance;
	}

	Session::Session() = default;

	// No networking destructor does real work during static destruction (§5.6):
	// Shutdown runs from close_game, before CloseWindow
	Session::~Session() = default;

	void Session::StartOffline()
	{
		Shutdown();
		mode = SessionMode::Offline;

		auto [serverEnd, clientEnd] = Net::LoopbackTransport::CreatePair();
		serverLoopback = std::move(serverEnd);
		clientLoopback = std::move(clientEnd);
		serverLoopback->Listen(0);

		Sim::ServerConfig config;
		config.maxPlayers = 1; // single-player: the one Slot is this game's
		server = std::make_unique<Sim::ServerWorld>(*serverLoopback, config);
		client = std::make_unique<ClientWorld>(*clientLoopback, false);
		SetLocalSlot(0);
		attached = nullptr;
		connected = false;
	}

	void Session::AttachOffline(SceneManagement::Scene* scene)
	{
		attached = scene;

		// One store at a time, one per World save: single-player's keeps its
		// World state for the session only (Q47)
		if (store) { store->Close(std::chrono::milliseconds(0)); }
		store = std::make_unique<World::FileWorldStore>();
		store->BeginOpen(World::MakeWorldKey(World::WorldIdFromName(scene->worldName), scene->contentHash));

		server->SetWorldStore(store.get());
		server->AttachScene(scene);
		server->ApplyWorldState();

		if (!connected)
		{
			client->Connect("loopback", scene->contentHash, "Player", scene->stations.Count());
			connected = true;
		}
		else
		{
			client->SetStationCount(scene->stations.Count());
		}
	}

	bool Session::Frozen(const SceneManagement::Scene& scene) const
	{
		// Single-player's Pause stops the world. While a remote player is connected
		// neither the Pause overlay nor the World Editor's pause stops the server (§5.6).
		return mode == SessionMode::Offline && scene.overlays.IsOpen(Overlay::Pause);
	}

	bool Session::LocalLook(const SceneManagement::Scene& scene, float& yaw, float& pitch) const
	{
		if (client && client->IsRemote())
		{
			yaw = client->PredictedYaw();
			pitch = client->PredictedPitch();
			return true;
		}
		if (const Player* local = LocalPlayer(scene))
		{
			yaw = local->lookYaw;
			pitch = local->lookPitch;
			return true;
		}
		return false;
	}

	void Session::Frame(SceneManagement::SceneManager& manager, double now)
	{
		SceneManagement::Scene* scene = manager.currentScene;
		if (scene == nullptr || !client) { return; }
		if (client->IsRemote()) { client->ApplySnapshots(*scene, now); }
	}

	void Session::Tick(SceneManagement::SceneManager& manager, const Sim::PlayerCommand& command)
	{
		SceneManagement::Scene* scene = manager.currentScene;
		if (scene == nullptr || !client) { return; }

		if (mode == SessionMode::Offline)
		{
			if (scene != attached) { AttachOffline(scene); }
			if (Frozen(*scene)) { return; } // paused: nothing is sent and nothing moves

			client->SendCommand(command);
			client->SendStationRequests(*scene);
			server->Tick(Sim::kTickDelta);
			client->Poll(scene);
			return;
		}
	}

	void Session::Shutdown()
	{
		// §5.6: stop ticking, close every connection, then the World store
		if (server) { server->DisconnectAll(); }
		client.reset();
		server.reset();
		clientLoopback.reset();
		serverLoopback.reset();
		if (store) { store->Close(std::chrono::milliseconds(0)); }
		store.reset();
		attached = nullptr;
		connected = false;
	}
}
