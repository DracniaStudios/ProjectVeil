#include "Session.h"

#include <ResourcePaths.h>
#include <Scene.h>
#include <SceneManager.h>
#include <Client/ClientWorld.h>
#include <Client/DevOptions.h>
#include <Client/LocalPlayer.h>
#include <World/FileWorldStore.h>
#include <World/WorldKey.h>
#include <net/LoopbackTransport.h>
#include <net/Protocol.h>
#include <net/spacetime/SpacetimeWorldStore.h>
#include <sim/FixedTick.h>
#include <sim/ServerWorld.h>
#if defined(VEIL_HAS_GNS)
#include <net/gns/GnsTransport.h>
#endif

#include <imgui.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Client {

	namespace {
		const std::filesystem::path& SavesDir() { return ResourcePaths::Saves(); }

		std::unique_ptr<Net::INetTransport> MakeGnsTransport()
		{
#if defined(VEIL_HAS_GNS)
			return Net::Gns::CreateTransport();
#else
			return nullptr;
#endif
		}

		// The world is ready to run once the pushed scene is current and whole
		bool SceneLoaded(SceneManagement::SceneManager& manager, SceneManagement::Scene* before)
		{
			return manager.currentScene != nullptr && manager.currentScene != before && manager.nextSceneID < 0;
		}
	}

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

	// ---- Single-player ------------------------------------------------------

	void Session::StartOffline()
	{
		StopNetworking();
		if (store) { store->Close(std::chrono::milliseconds(0)); }
		store.reset();
		mode = SessionMode::Offline;
		phase = SessionPhase::Running;

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
		sessionScene = nullptr;
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

	// ---- Starting a networked session ---------------------------------------

	void Session::StartHost()
	{
#if defined(VEIL_DEV_NET) && defined(VEIL_HAS_GNS)
		if (IsNetworked()) { return; }
		LoadDevNet();
		lastError.clear();

		StopNetworking();
		std::string error;
		if (!Net::Gns::Init(error)) { Fail("GameNetworkingSockets didn't start: " + error); return; }
		gnsStarted = true;
		if (fakeLagMs > 0 || fakeLossPercent > 0.0f) { Net::Gns::SetFakeNetwork(fakeLagMs, fakeLossPercent); }

		mode = SessionMode::Host;
		phase = SessionPhase::LoadingWorld;
		status = "Loading the world…";
		auto& manager = SceneManagement::SceneManager::getInstance();
		sceneBefore = manager.currentScene;
		SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);
		std::cout << "[Session] Starting a Host\n";
#else
		Fail("This build has no development multiplayer (VEIL_DEV_NET) or no GameNetworkingSockets");
#endif
	}

	void Session::StartJoin(const std::string& target, const std::string& playerName)
	{
#if defined(VEIL_DEV_NET) && defined(VEIL_HAS_GNS)
		if (IsNetworked()) { return; }
		LoadDevNet();
		lastError.clear();
		address = target.empty() ? std::string(":") + std::to_string(Net::kDefaultPort) : target;
		name = Net::IsValidName(playerName) && !playerName.empty() ? playerName : std::string("Player");
		SaveDevNet();

		StopNetworking();
		std::string error;
		if (!Net::Gns::Init(error)) { Fail("GameNetworkingSockets didn't start: " + error); return; }
		gnsStarted = true;
		if (fakeLagMs > 0 || fakeLossPercent > 0.0f) { Net::Gns::SetFakeNetwork(fakeLagMs, fakeLossPercent); }

		// A joining client loads the World save fresh, never its own Game save (P-8)
		mode = SessionMode::Join;
		phase = SessionPhase::LoadingWorld;
		status = "Loading the world…";
		auto& manager = SceneManagement::SceneManager::getInstance();
		sceneBefore = manager.currentScene;
		SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);
		std::cout << "[Session] Joining " << address << " as \"" << name << "\"\n";
#else
		(void)target;
		(void)playerName;
		Fail("This build has no development multiplayer (VEIL_DEV_NET) or no GameNetworkingSockets");
#endif
	}

	void Session::FrameHost(SceneManagement::SceneManager& manager, double now)
	{
		switch (phase)
		{
		case SessionPhase::LoadingWorld:
		{
			if (!SceneLoaded(manager, sceneBefore)) { return; }
			sessionScene = manager.currentScene;

			// W-8: the world_id is checked as the config loads; the Host refuses bad ones
			worldId = !Dev().worldId.empty() ? Dev().worldId : World::WorldIdFromName(sessionScene->worldName);
			if (!World::IsValidWorldId(worldId)) { Fail("world_id \"" + worldId + "\" isn't [a-z0-9_]{1,32}"); return; }

			// P-3: refuse to host a World save over the protocol's limits
			int stationsInMap = 0;
			sessionScene->gameMap.ForEachInteractable([&](InteractableObject& o) { if (o.interactType == INTERACT_MINIGAME) { ++stationsInMap; } });
			if (stationsInMap > Net::kMaxStations) { Fail("the World save has " + std::to_string(stationsInMap) + " Task stations; the protocol carries " + std::to_string(Net::kMaxStations)); return; }

			Net::Spacetime::StoreConfig config;
			config.identityFile = SavesDir() / "dev_identity.json";
			config.lockFile = SavesDir() / "host.lock";
			store = std::make_unique<Net::Spacetime::SpacetimeWorldStore>(config);
			store->BeginOpen(World::MakeWorldKey(worldId, sessionScene->contentHash));
			phase = SessionPhase::OpeningStore;
			status = "Opening world…";
			return;
		}

		case SessionPhase::OpeningStore:
		{
			const World::WorldStoreStatus storeStatus = store->Status();
			if (storeStatus == World::WorldStoreStatus::Opening) { return; }
			if (storeStatus == World::WorldStoreStatus::Failed)
			{
				auto* spacetime = static_cast<Net::Spacetime::SpacetimeWorldStore*>(store.get());
				Fail("Start Server failed: " + spacetime->LastError());
				return;
			}

			// W-6: the World state is applied before the first tick and before listening
			serverTransport = MakeGnsTransport();
			Sim::ServerConfig config;
			config.maxPlayers = Sim::kDefaultMaxPlayers;
			server = std::make_unique<Sim::ServerWorld>(*serverTransport, config);
			server->SetWorldStore(store.get());
			server->AttachScene(sessionScene);
			if (!server->ApplyWorldState()) { Fail("the World state couldn't be read"); return; }
			if (!serverTransport->Listen(Net::kDefaultPort)) { Fail("couldn't listen on port " + std::to_string(Net::kDefaultPort)); return; }

			// The Host's own client joins over GNS too, so the server has one
			// transport (§5.1); it draws the server's own scene (Attached)
			clientTransport = MakeGnsTransport();
			client = std::make_unique<ClientWorld>(*clientTransport, false);
			if (!client->Connect("127.0.0.1:" + std::to_string(Net::kDefaultPort), sessionScene->contentHash, name, sessionScene->stations.Count()))
			{
				Fail("the Host's own client couldn't connect");
				return;
			}
			phase = SessionPhase::Connecting;
			status = "Connecting…";
			savingDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			return;
		}

		case SessionPhase::Connecting:
			if (client->State() == ConnectionState::Welcomed)
			{
				phase = SessionPhase::Running;
				nextBandwidthLog = now + 10.0;
				std::cout << "[Session] Hosting on port " << Net::kDefaultPort << ", world " << worldId << "\n";
			}
			else if (client->State() == ConnectionState::Rejected || client->State() == ConnectionState::Lost
				|| std::chrono::steady_clock::now() > savingDeadline)
			{
				Fail("the Host's own client couldn't join its server");
			}
			return;

		case SessionPhase::Running:
			if (manager.currentScene != sessionScene) { Leave(); return; } // the end card's or Pause's Main Menu
			LogBandwidth(now);
			return;

		default:
			return;
		}
	}

	void Session::FrameJoin(SceneManagement::SceneManager& manager, double now)
	{
		switch (phase)
		{
		case SessionPhase::LoadingWorld:
			if (!SceneLoaded(manager, sceneBefore)) { return; }
			sessionScene = manager.currentScene;
			clientTransport = MakeGnsTransport();
			client = std::make_unique<ClientWorld>(*clientTransport, true);
			if (!client->Connect(address, sessionScene->contentHash, name, sessionScene->stations.Count()))
			{
				Fail("\"" + address + "\" isn't an address to join");
				return;
			}
			phase = SessionPhase::Connecting;
			status = "Connecting to " + address + "…";
			savingDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			return;

		case SessionPhase::Connecting:
			if (client->State() == ConnectionState::Welcomed)
			{
				phase = SessionPhase::Running;
				nextBandwidthLog = now + 10.0;
			}
			else if (client->State() == ConnectionState::Rejected)
			{
				Fail(std::string("Rejected by the Host: ") + Net::RejectReasonName(client->RejectedFor()));
			}
			else if (client->State() == ConnectionState::Lost || std::chrono::steady_clock::now() > savingDeadline)
			{
				Fail("Couldn't connect to " + address);
			}
			return;

		case SessionPhase::Running:
			if (client->State() == ConnectionState::Lost) { Fail("Disconnected from the Host"); return; }
			if (manager.currentScene != sessionScene) { Leave(); return; }
			client->ApplySnapshots(*sessionScene, now);
			LogBandwidth(now);
			return;

		default:
			return;
		}
	}

	// ---- Stopping -----------------------------------------------------------

	void Session::StopNetworking()
	{
		// §5.6: 1. stop ticking (the caller no longer ticks), 2. close every
		// connection with linger and the listen socket and poll group,
		// 3. GameNetworkingSockets_Kill. The World store closes after.
		if (server) { server->DisconnectAll(); }
		client.reset();
		server.reset();
		clientTransport.reset();
		serverTransport.reset();
		clientLoopback.reset();
		serverLoopback.reset();
#if defined(VEIL_HAS_GNS)
		if (gnsStarted) { Net::Gns::Kill(); }
#endif
		gnsStarted = false;
	}

	void Session::Leave(const std::string& why)
	{
		if (!IsNetworked() || phase == SessionPhase::Saving) { return; }
		if (!why.empty()) { lastError = why; }
		std::cout << "[Session] Leaving the " << SessionModeName(mode) << " session" << (why.empty() ? "" : ": " + why) << "\n";

		auto& manager = SceneManagement::SceneManager::getInstance();
		leaveToMenu = manager.currentScene != nullptr && manager.currentScene == sessionScene;

		StopNetworking();

		// 4. The World store flushes with "Saving…" up, for at most 5 s (W-7)
		if (store && store->Pending() > 0)
		{
			phase = SessionPhase::Saving;
			status = "Saving…";
			savingDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			return;
		}
		FrameSaving(manager);
	}

	void Session::FrameSaving(SceneManagement::SceneManager& manager)
	{
		if (store && store->Pending() > 0 && std::chrono::steady_clock::now() < savingDeadline) { return; }
		if (store) { store->Close(std::chrono::milliseconds(0)); }
		store.reset();

		const bool toMenu = leaveToMenu;
		StartOffline();
		if (toMenu) { SceneManagement::SceneManager_push(&manager, SCENE_MAIN_MENU); }
	}

	void Session::Fail(const std::string& why)
	{
		std::cerr << "[Session] " << why << "\n";
		lastError = why;
		auto& manager = SceneManagement::SceneManager::getInstance();
		leaveToMenu = IsNetworked() && manager.currentScene != nullptr && manager.currentScene != sceneBefore;

		StopNetworking();
		if (store) { store->Close(std::chrono::milliseconds(0)); }
		store.reset();

		const bool toMenu = leaveToMenu;
		StartOffline();
		if (toMenu) { SceneManagement::SceneManager_push(&manager, SCENE_MAIN_MENU); }
	}

	void Session::Shutdown()
	{
		StopNetworking();
		// The window is closing: block for the flush, at most 5 s (§5.6 step 4)
		if (store) { store->Close(std::chrono::seconds(5)); }
		store.reset();
		attached = nullptr;
		sessionScene = nullptr;
		connected = false;
		mode = SessionMode::Offline;
		phase = SessionPhase::Idle;
	}

	// ---- Every frame and tick -----------------------------------------------

	void Session::Frame(SceneManagement::SceneManager& manager, double now)
	{
		// The dev switches, once the game is up
		static bool startedFromSwitches = false;
		if (!startedFromSwitches && manager.currentScene != nullptr)
		{
			startedFromSwitches = true;
			// dev_net.json first, so the switches win over what it remembers
			LoadDevNet();
			if (Dev().fakeLagMs > 0 || Dev().fakeLossPercent > 0.0f) { fakeLagMs = Dev().fakeLagMs; fakeLossPercent = Dev().fakeLossPercent; }
			if (!Dev().name.empty()) { name = Dev().name; }
			if (Dev().host) { StartHost(); }
			else if (!Dev().join.empty()) { StartJoin(Dev().join, Dev().name); }
		}

		if (phase == SessionPhase::Saving) { FrameSaving(manager); return; }
		if (mode == SessionMode::Host) { FrameHost(manager, now); }
		else if (mode == SessionMode::Join) { FrameJoin(manager, now); }
	}

	bool Session::Frozen(const SceneManagement::Scene& scene) const
	{
		// Single-player's Pause stops the world. While a remote player is connected
		// neither the Pause overlay nor the World Editor's pause stops the server (§5.6).
		if (mode == SessionMode::Offline) { return scene.overlays.IsOpen(Overlay::Pause); }
		return phase != SessionPhase::Connecting && phase != SessionPhase::Running;
	}

	bool Session::LocalLook(const SceneManagement::Scene& scene, float& yaw, float& pitch) const
	{
		if (client && client->IsRemote() && phase == SessionPhase::Running)
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

		if (Frozen(*scene) || scene != sessionScene) { return; }

		client->SendCommand(command);
		client->SendStationRequests(*scene);
		if (server) { server->Tick(Sim::kTickDelta); } // the Host's own, in-process
		client->Poll(scene);
	}

	// ---- Status, bandwidth, the dev windows ---------------------------------

	std::string Session::StatusLine() const
	{
		switch (phase)
		{
		case SessionPhase::Saving: return "Saving…";
		case SessionPhase::LoadingWorld:
		case SessionPhase::OpeningStore:
		case SessionPhase::Connecting:
			return status;
		default:
			break;
		}

		if (mode == SessionMode::Host && server)
		{
			char line[96];
			std::snprintf(line, sizeof(line), "Hosting :%u (%d/%d)", Net::kDefaultPort, server->WelcomedCount(), server->MaxPlayers());
			return line;
		}
		if (mode == SessionMode::Join && client && clientTransport)
		{
			const Net::NetStats stats = clientTransport->Stats(client->Connection());
			char line[128];
			std::snprintf(line, sizeof(line), "Connected %s, %.0f ms", address.c_str(), stats.pingMs);
			return line;
		}
		return lastError.empty() ? std::string("Idle") : lastError;
	}

	void Session::LogBandwidth(double now)
	{
		// Per-client bytes/s every 10 s, for the test log's bandwidth row (N8)
		if (now < nextBandwidthLog) { return; }
		nextBandwidthLog = now + 10.0;

		if (mode == SessionMode::Host && server && serverTransport)
		{
			for (const Sim::ServerWorld::PeerInfo& peer : server->Peers())
			{
				if (!peer.welcomed) { continue; }
				const Net::NetStats s = serverTransport->Stats(peer.connection);
				std::printf("[Net] Slot %d \"%s\": out %.0f B/s, in %.0f B/s, ping %.0f ms, loss %.1f%%\n",
					peer.slot, peer.name.c_str(), s.bytesOutPerSec, s.bytesInPerSec, s.pingMs, s.packetLoss * 100.0f);
			}
		}
		else if (mode == SessionMode::Join && client && clientTransport)
		{
			const Net::NetStats s = clientTransport->Stats(client->Connection());
			std::printf("[Net] Client Slot %d: in %.0f B/s, out %.0f B/s, ping %.0f ms, loss %.1f%%, snapshots dropped %d\n",
				client->Slot(), s.bytesInPerSec, s.bytesOutPerSec, s.pingMs, s.packetLoss * 100.0f, client->DroppedSnapshots());
		}
		std::fflush(stdout);
	}

	void Session::LoadDevNet()
	{
		if (devNetLoaded) { return; }
		devNetLoaded = true;

		// dev_net.json, next to the settings file: the last address, the name, the
		// faked network. Settings stores only numbers, hence its own file (Step 4.9).
		std::ifstream in(SavesDir() / "dev_net.json");
		if (!in) { return; }
		const Json json = Json::parse(in, nullptr, false);
		if (json.is_discarded() || !json.is_object()) { return; }
		const std::string last = json.value("lastAddress", std::string(":7777"));
		const std::string savedName = json.value("name", std::string("Player"));
		std::snprintf(addressBuffer, sizeof(addressBuffer), "%s", last.c_str());
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", savedName.c_str());
		fakeLagMs = json.value("fakeLagMs", 0);
		fakeLossPercent = json.value("fakeLossPercent", 0.0f);
	}

	void Session::SaveDevNet() const
	{
		std::error_code ignored;
		std::filesystem::create_directories(SavesDir(), ignored);
		std::ofstream out(SavesDir() / "dev_net.json", std::ios::trunc);
		if (!out) { return; }
		Json json;
		json["lastAddress"] = address;
		json["name"] = name;
		json["fakeLagMs"] = fakeLagMs;
		json["fakeLossPercent"] = fakeLossPercent;
		out << json.dump(1, '\t');
	}

	void Session::DrawUi()
	{
#if defined(VEIL_DEV_NET)
		if (joinWindowOpen)
		{
			LoadDevNet();
			ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Join Server", &joinWindowOpen))
			{
				ImGui::TextUnformatted("Development build: LAN or direct IP");
				ImGui::InputText("Address", addressBuffer, sizeof(addressBuffer));
				ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
				if (!Net::IsValidName(nameBuffer)) { ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "At most 24 printable ASCII characters"); }
				if (ImGui::Button("Connect") && Net::IsValidName(nameBuffer))
				{
					joinWindowOpen = false;
					StartJoin(addressBuffer, nameBuffer);
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) { joinWindowOpen = false; }
			}
			ImGui::End();
		}

		if (!IsNetworked() && phase != SessionPhase::Saving) { return; }

		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 540, 20), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Network"))
		{
			const std::string line = StatusLine();
			ImGui::TextUnformatted(line.c_str());

			if (client && client->State() == ConnectionState::Welcomed)
			{
				ImGui::Text("Slot %d, server tick %u, epoch %u", client->Slot(), client->NewestServerTick(), client->RoundEpoch());
			}
			if (mode == SessionMode::Host && server)
			{
				for (const Sim::ServerWorld::PeerInfo& peer : server->Peers())
				{
					if (!peer.welcomed) { continue; }
					ImGui::Text("Slot %d:", peer.slot);
					ImGui::SameLine();
					ImGui::TextUnformatted(peer.name.c_str()); // P-9: names are drawn unformatted
				}
				if (store)
				{
					const char* storeState = "?";
					switch (store->Status())
					{
					case World::WorldStoreStatus::Opening:  storeState = "Opening"; break;
					case World::WorldStoreStatus::Ready:    storeState = "Ready"; break;
					case World::WorldStoreStatus::Degraded: storeState = "Degraded: SpacetimeDB isn't answering; writes are queued"; break;
					case World::WorldStoreStatus::Failed:   storeState = "Failed"; break;
					case World::WorldStoreStatus::Closed:   storeState = "Closed"; break;
					}
					ImGui::Text("World store: %s (%zu pending)", storeState, store->Pending());
				}
			}

#if defined(VEIL_HAS_GNS)
			// Faked latency and loss through GNS's own simulation (Step 4.11)
			bool changed = ImGui::SliderInt("Fake lag (ms)", &fakeLagMs, 0, 300);
			changed |= ImGui::SliderFloat("Fake loss (%)", &fakeLossPercent, 0.0f, 10.0f, "%.1f");
			if (changed && gnsStarted) { Net::Gns::SetFakeNetwork(fakeLagMs, fakeLossPercent); }
#endif

			if (phase != SessionPhase::Saving && ImGui::Button("Leave")) { Leave(); }
		}
		ImGui::End();
#endif
	}
}
