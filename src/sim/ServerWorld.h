#pragma once
#ifndef SIM_SERVER_WORLD_H
#define SIM_SERVER_WORLD_H

// The server half (DedicatedServerPlan.md Step 2.2; MultiplayerSplitPlan.md N2,
// N5, G7). It owns the simulation of one Scene: it reads every connection,
// turns each Slot's commands into one tick, sends what happened, and keeps the
// World store in step. In single-player it runs in the game's own process over
// Loopback; on a Host the same code listens on GNS.
//
// Rules it keeps:
//   R2  players are Slots; nothing here knows which one is "local".
//   R3  world state changes only inside Tick.
//   P-5 the Slot comes from the connection, never from a message; only Hello
//       before Welcome; no Hello within 5 s closes; unwelcomed connections are
//       capped at maxPlayers.
//   P-6 commands: any seq > last is accepted, a queue of 8 per Slot drops the
//       oldest, exactly one is applied per tick, an empty queue repeats the last
//       held input without its presses.
//   C-1 the round is decided once per tick (Scene::ResolveRound); the server
//       sends what it decided and writes the Full wipe through.

#include <net/NetTransport.h>
#include <net/Protocol.h>
#include <sim/PlayerCommand.h>
#include <sim/Slots.h>
#include <sim/StationTable.h>
#include <World/WorldStore.h>

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace SceneManagement { struct Scene; }

namespace Sim {

	struct ServerConfig
	{
		int maxPlayers = kDefaultMaxPlayers;      // at most kMaxPlayers (Q22 defaults)
		int helloTimeoutTicks = 5 * 60;           // P-5: 5 s at 60 Hz
		int pollBudgetPerConnection = 32;         // P-7: messages per tick before a connection is closed
		int commandQueueCap = 8;                  // P-6
		int stationRequestsPerSecond = 4;         // S-4: a Slot over this is disconnected
		int snapshotEveryTicks = 2;               // 60 Hz / 30 Hz
	};

	class ServerWorld
	{
	public:
		explicit ServerWorld(Net::INetTransport& transport, ServerConfig config = {});

		// The World store World state is read from and written through (Q48). A
		// server with none keeps World state in memory only.
		void SetWorldStore(World::WorldStore* store) { worldStore = store; }

		/**
		 * The scene this server simulates. Builds the StationTable and the content
		 * hash from it, and gives every connected Slot a player in it. Call again
		 * whenever the SceneManager builds a new scene. World state is applied
		 * separately (ApplyWorldState), once the store has it.
		 */
		void AttachScene(SceneManagement::Scene* scene);
		SceneManagement::Scene* AttachedScene() const { return scene; }

		/**
		 * Applies the World store's state to the attached scene, once (W-6): the
		 * completed stations and the Targets derived from them. False while the
		 * store isn't Ready or Degraded yet; true once applied, or when there is no
		 * store. A Host calls this before its first tick and before it listens.
		 */
		bool ApplyWorldState();
		bool WorldStateApplied() const { return worldStateApplied; }

		// One fixed tick: connections, commands, the simulation, events, Snapshots
		void Tick(float deltaTime, bool simulationFrozen = false);

		std::uint32_t ServerTick() const { return serverTick; }
		std::uint64_t WorldHash() const { return worldHash; }
		const Net::Limits& WireLimits() const { return limits; }
		int MaxPlayers() const { return config.maxPlayers; }

		struct PeerInfo
		{
			Net::ConnectionId connection = Net::kInvalidConnection;
			int slot = -1;
			bool welcomed = false;
			std::string name;
		};
		std::vector<PeerInfo> Peers() const;
		int WelcomedCount() const;

		// Why the last connection that went away was closed, for the log and tests
		const std::string& LastCloseReason() const { return lastCloseReason; }

		// Closes every connection (with linger on GNS) and forgets them
		void DisconnectAll();

	private:
		struct Peer
		{
			Net::ConnectionId connection = Net::kInvalidConnection;
			int slot = -1;
			bool welcomed = false;
			std::uint32_t connectedTick = 0;
			std::string name;

			std::uint32_t lastSeq = 0;      // highest command sequence accepted
			std::uint32_t appliedSeq = 0;   // last one applied: the Snapshot's ackSeq
			std::deque<PlayerCommand> queue;
			PlayerCommand lastApplied = {};

			int messagesThisTick = 0;
			std::uint32_t requestWindowTick = 0;
			int requestsInWindow = 0;
			bool closing = false;
		};

		Peer* FindPeer(Net::ConnectionId connection);
		Peer* PeerInSlot(int slot);
		int FreeSlot() const;

		void PollTransport();
		void OnConnected(Net::ConnectionId connection);
		void OnDisconnected(Net::ConnectionId connection);
		void OnMessage(Peer& peer, std::span<const std::byte> bytes);
		void OnHello(Peer& peer, const Net::HelloMsg& hello);
		void OnCommands(Peer& peer, const Net::CommandsMsg& commands);
		void OnStationRequest(Peer& peer, const Net::StationRequestMsg& request);

		void Close(Peer& peer, const std::string& reason);
		void Reject(Peer& peer, Net::RejectReason reason);
		void Send(const Peer& peer, const Net::Message& message, Net::Delivery delivery);
		void Broadcast(const Net::Message& message, Net::Delivery delivery);

		void CloseTimedOutHellos();
		std::array<PlayerCommand, kMaxPlayers> TakeCommands();
		void SendEvents();
		void SendSnapshots();
		void ReleaseStationsHeldBy(int slot, bool notify);

		Net::INetTransport& transport;
		ServerConfig config;
		World::WorldStore* worldStore = nullptr;
		SceneManagement::Scene* scene = nullptr;

		Net::Limits limits;
		std::uint64_t worldHash = 0;
		bool worldStateApplied = false;

		std::vector<Peer> peers;
		std::uint32_t serverTick = 0;
		std::string lastCloseReason;

		Net::Encoder encoder;
		Net::Decoder decoder;
	};
}

#endif
