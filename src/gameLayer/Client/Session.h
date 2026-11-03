#pragma once
#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

// What this game is connected to (MultiplayerSplitPlan.md N2, N7; §5.6).
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
// command reaches the simulation in the tick it was sampled for.

#include <sim/PlayerCommand.h>

#include <cstdint>
#include <memory>
#include <string>

namespace SceneManagement { struct Scene; struct SceneManager; }
namespace Sim { class ServerWorld; }
namespace Net { class INetTransport; class LoopbackTransport; }
namespace World { class WorldStore; }

namespace Client {

	class ClientWorld;

	enum class SessionMode { Offline, Host, Join };

	const char* SessionModeName(SessionMode mode);

	class Session
	{
	public:
		static Session& getInstance();

		// Single-player, the game's default
		void StartOffline();

		// Per frame, before the scene's frame: a remote client's Snapshots are
		// applied, and anything waiting (a World store opening, a store saving) moves on
		void Frame(SceneManagement::SceneManager& manager, double now);

		// One fixed tick
		void Tick(SceneManagement::SceneManager& manager, const Sim::PlayerCommand& command);

		// Stops everything networked, in the §5.6 order. Called before CloseWindow.
		void Shutdown();

		SessionMode Mode() const { return mode; }
		ClientWorld* Client() { return client.get(); }
		Sim::ServerWorld* Server() { return server.get(); }
		World::WorldStore* Store() { return store.get(); }

		// The look the local player's camera starts from this frame: the
		// simulated look in single-player and on the Host, the predicted look on
		// a joined client
		bool LocalLook(const SceneManagement::Scene& scene, float& yaw, float& pitch) const;

		// Whether the simulation is frozen this tick (single-player's Pause)
		bool Frozen(const SceneManagement::Scene& scene) const;

		~Session();

	private:
		Session();

		void AttachOffline(SceneManagement::Scene* scene);

		SessionMode mode = SessionMode::Offline;

		std::unique_ptr<Net::LoopbackTransport> serverLoopback;
		std::unique_ptr<Net::LoopbackTransport> clientLoopback;
		std::unique_ptr<World::WorldStore> store;
		std::unique_ptr<Sim::ServerWorld> server;
		std::unique_ptr<ClientWorld> client;

		SceneManagement::Scene* attached = nullptr;
		bool connected = false;
	};
}

#endif
