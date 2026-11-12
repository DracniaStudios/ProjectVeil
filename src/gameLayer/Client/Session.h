#pragma once
#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

// What this game is connected to (MultiplayerSplitPlan.md N2, N7, N8; §5.6).
//
//   Offline  single-player: the in-process ServerWorld and this ClientWorld,
//            joined by Loopback, with a FileWorldStore. No socket, no SpacetimeDB.
//   Host     Start Server: the in-process ServerWorld listens on GNS, this
//            game's own client joins it over GNS on 127.0.0.1 as Slot 0, and the
//            World state lives in the Host PC's SpacetimeDB.
//   Join     a client only, drawing its own copy of the World save from the
//            Host's Snapshots.
//
// The main loop hands it one command per tick; it runs the client's send, the
// server's tick and the client's receive in that order, so in single-player a
// command reaches the simulation in the tick it was sampled for. Starting and
// stopping a networked session never blocks a frame: "Opening world…",
// "Connecting…" and "Saving…" are phases the window keeps drawing through.

#include <sim/PlayerCommand.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SceneManagement { struct Scene; struct SceneManager; }
namespace Sim { class ServerWorld; }
namespace Net { class INetTransport; class LoopbackTransport; }
namespace World { class WorldStore; }

namespace Client {

	class ClientWorld;

	enum class SessionMode { Offline, Host, Join };

	enum class SessionPhase { Idle, LoadingWorld, OpeningStore, Connecting, Running, Saving };

	const char* SessionModeName(SessionMode mode);

	class Session
	{
	public:
		static Session& getInstance();

		// Single-player, the game's default
		void StartOffline();

		// Start Server: host on Net::kDefaultPort and join as Slot 0 (Step 4.9).
		// Needs SpacetimeDB running on the Host PC (spacetime/start_spacetime.ps1).
		void StartHost();

		// Join Server: connect to "ip[:port]" with this name
		void StartJoin(const std::string& address, const std::string& name);

		// Back to single-player: a Host saves first, without blocking a frame
		void Leave(const std::string& why = "");

		// Per frame, before the scene's frame: moves the start-up and shut-down
		// phases on, applies a remote client's Snapshots, logs bandwidth
		void Frame(SceneManagement::SceneManager& manager, double now);

		// One fixed tick
		void Tick(SceneManagement::SceneManager& manager, const Sim::PlayerCommand& command);

		// The development multiplayer windows: status, Join, faked network (ImGui)
		void DrawUi();
		void ShowJoinWindow() { joinWindowOpen = true; }

		// Stops everything networked, in the §5.6 order, blocking until the World
		// store has flushed or 5 s passed. Called from close_game, before CloseWindow.
		void Shutdown();

		// The SceneManager is about to free `scene`: the server lets go of it, an Offline
		// session re-attaches on its next tick, and a session still starting on it fails.
		void ForgetScene(SceneManagement::Scene* scene);

		SessionMode Mode() const { return mode; }
		SessionPhase Phase() const { return phase; }
		bool IsNetworked() const { return mode != SessionMode::Offline; }
		ClientWorld* Client() { return client.get(); }
		Sim::ServerWorld* Server() { return server.get(); }
		World::WorldStore* Store() { return store.get(); }

		// One line for the Multiplayer overlay: "Idle", "Hosting :7777 (2/4)",
		// "Connected 192.168.1.50:7777, 3 ms", "Saving…", or the last error
		std::string StatusLine() const;

		// The look the local player's camera starts from this frame: the
		// simulated look in single-player and on the Host, the predicted look on
		// a joined client
		bool LocalLook(const SceneManagement::Scene& scene, float& yaw, float& pitch) const;

		// Whether the simulation is frozen this tick: single-player's Pause, and
		// any session while it is starting or stopping
		bool Frozen(const SceneManagement::Scene& scene) const;

		~Session();

	private:
		Session();

		void AttachOffline(SceneManagement::Scene* scene);
		void FrameHost(SceneManagement::SceneManager& manager, double now);
		void FrameJoin(SceneManagement::SceneManager& manager, double now);
		void FrameSaving(SceneManagement::SceneManager& manager);
		void Fail(const std::string& why);
		void StopNetworking();   // §5.6 steps 1-3
		void LogBandwidth(double now);
		void LoadDevNet();
		void SaveDevNet() const;

		SessionMode mode = SessionMode::Offline;
		SessionPhase phase = SessionPhase::Idle;

		std::unique_ptr<Net::LoopbackTransport> serverLoopback;
		std::unique_ptr<Net::LoopbackTransport> clientLoopback;
		std::unique_ptr<Net::INetTransport> serverTransport; // GNS, Host
		std::unique_ptr<Net::INetTransport> clientTransport; // GNS, Host's own client or Join
		std::unique_ptr<World::WorldStore> store;
		std::unique_ptr<Sim::ServerWorld> server;
		std::unique_ptr<ClientWorld> client;
		bool gnsStarted = false;

		SceneManagement::Scene* attached = nullptr;      // Offline: the scene the server runs
		SceneManagement::Scene* sceneBefore = nullptr;   // the scene a networked start began on
		SceneManagement::Scene* sessionScene = nullptr;  // the scene a networked session runs on
		bool connected = false;

		std::string address;
		std::string name = "Player";
		std::string worldId;
		std::string onlineWorldKey;                    // Join: names the temp file of the Host's World state
		std::vector<std::uint64_t> onlineSyncedStations; // what that file holds, so it is rewritten only on change
		bool onlineSynced = false;
		std::string status;
		std::string lastError;
		std::chrono::steady_clock::time_point savingDeadline;
		bool leaveToMenu = false;
		double nextBandwidthLog = 0.0;

		// The Join window
		bool joinWindowOpen = false;
		char addressBuffer[64] = ":7777";
		char nameBuffer[32] = "Player";
		int fakeLagMs = 0;
		float fakeLossPercent = 0.0f;
		bool devNetLoaded = false;
	};
}

#endif
