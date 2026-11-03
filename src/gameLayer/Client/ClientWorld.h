#pragma once
#ifndef CLIENT_CLIENT_WORLD_H
#define CLIENT_CLIENT_WORLD_H

// The client half (DedicatedServerPlan.md Step 2.2; MultiplayerSplitPlan.md N2,
// N6; rule P-8). It sends this game's commands and station requests, and turns
// what the server sends into what the player sees and hears.
//
// Two ways to draw:
//   Attached  single-player and the Host's own client: the server runs in this
//             process on the very Scene that is drawn, so Snapshots only carry
//             the ack and the round's epoch.
//   Remote    a joined client: the Scene is this game's own copy of the World
//             save, never ticked. Snapshots move it, drawn 100 ms behind the
//             newest one, interpolated (other players) or at the newest (the
//             local player, who has no prediction).

#include <net/NetTransport.h>
#include <net/Protocol.h>
#include <sim/PlayerCommand.h>

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

namespace SceneManagement { struct Scene; }

namespace Client {

	enum class ConnectionState { Idle, Connecting, Welcomed, Rejected, Lost };

	const char* ConnectionStateName(ConnectionState state);

	class ClientWorld
	{
	public:
		ClientWorld(Net::INetTransport& transport, bool remote);

		bool IsRemote() const { return remote; }

		// The StationTable's size every station index is checked against. An
		// Offline session moving to another scene sets it again.
		void SetStationCount(int count) { limits.stationCount = count; }

		/**
		 * Starts connecting. Hello goes out once the transport reports the
		 * connection, carrying the World save's content hash, so a client with
		 * another World save is refused (WorldMismatch).
		 */
		bool Connect(std::string_view address, std::uint64_t worldHash, std::string name, int stationCount);

		// Per tick, before the server's tick: sends this command with the two
		// before it, so one lost packet costs nothing
		void SendCommand(const Sim::PlayerCommand& command);

		// Sends what the scene's MiniGame screen asked of the stations
		void SendStationRequests(SceneManagement::Scene& scene);

		// Per tick, after the server's tick: reads everything that arrived. Events
		// are shown at once (Client::PresentEvent); Snapshots are kept for drawing.
		void Poll(SceneManagement::Scene* scene);

		// Remote only, per frame: moves the scene to where the server was 100 ms
		// ago (the local player to the newest Snapshot). `now` in seconds.
		void ApplySnapshots(SceneManagement::Scene& scene, double now);

		// The local player's look, which a remote client turns at once instead of
		// waiting a round trip for the server's Snapshot
		float PredictedYaw() const { return predictedYaw; }
		float PredictedPitch() const { return predictedPitch; }

		ConnectionState State() const { return state; }
		int Slot() const { return slot; }
		int MaxPlayers() const { return maxPlayers; }
		Net::RejectReason RejectedFor() const { return rejectReason; }
		std::uint32_t NewestServerTick() const { return newestServerTick; }
		std::uint32_t AckedSequence() const { return ackedSequence; }
		std::uint32_t RoundEpoch() const { return knownEpoch; }
		double NewestSnapshotAt() const { return newestSnapshotAt; }
		Net::ConnectionId Connection() const { return connection; }

		// Snapshots dropped for arriving late or breaking a rule (P-8)
		int DroppedSnapshots() const { return droppedSnapshots; }

		// How far behind the newest Snapshot a remote client draws (Step 4.6)
		static constexpr double kInterpolationDelay = 0.1;

	private:
		struct Received
		{
			Net::SnapshotMsg snapshot;
			double serverTime = 0.0; // serverTick in seconds
		};

		void OnMessage(SceneManagement::Scene* scene, std::span<const std::byte> bytes);
		void OnSnapshot(SceneManagement::Scene* scene, const Net::SnapshotMsg& snapshot);
		void EndRoundNow(SceneManagement::Scene* scene, std::uint32_t epoch);
		void Send(const Net::Message& message, Net::Delivery delivery);

		Net::INetTransport& transport;
		bool remote = false;

		ConnectionState state = ConnectionState::Idle;
		Net::ConnectionId connection = Net::kInvalidConnection;
		std::uint64_t worldHash = 0;
		std::string name;
		Net::Limits limits;
		int slot = -1;
		int maxPlayers = 0;
		Net::RejectReason rejectReason = Net::RejectReason::VersionMismatch;

		std::array<Sim::PlayerCommand, Net::kMaxCommandsPerMessage> recent = {};
		int recentCount = 0;

		std::uint32_t newestServerTick = 0;
		std::uint32_t ackedSequence = 0;
		std::uint32_t knownEpoch = 0;
		bool haveEpoch = false;
		double newestSnapshotAt = 0.0;
		int droppedSnapshots = 0;

		// Remote: the timeline the scene is drawn from
		std::deque<Received> snapshots;
		double clockOffset = 0.0; // local seconds minus server seconds, at its smallest
		bool haveClock = false;
		double lastApplyNow = 0.0;

		float predictedYaw = 0.0f;
		float predictedPitch = 0.0f;
		bool lookSeeded = false;

		Net::Encoder encoder;
		Net::Decoder decoder;
	};
}

#endif
