#include "ServerWorld.h"

#include <Scene.h>
#include <AI/Stalker.h>
#include <sim/FixedTick.h>

#include <algorithm>
#include <cmath>
#include <iostream>

static_assert(STALKER_STATE_COUNT == Net::kStalkerStateCount, "a Snapshot carries exactly the FSM's states");

namespace Sim {

	ServerWorld::ServerWorld(Net::INetTransport& transport, ServerConfig config)
		: transport(transport), config(config)
	{
		this->config.maxPlayers = std::clamp(config.maxPlayers, 1, kMaxPlayers);
	}

	// ---- Scene and World state ---------------------------------------------

	void ServerWorld::AttachScene(SceneManagement::Scene* attached)
	{
		scene = attached;
		worldStateApplied = false;
		if (scene == nullptr) { return; }

		// The table and hash were built as the World save loaded (Scene::OnWorldLoaded)
		limits.stationCount = scene->stations.Count();
		worldHash = scene->contentHash;

		// Every welcomed Slot has a player in the new scene
		for (const Peer& peer : peers)
		{
			if (peer.welcomed) { scene->AddPlayer(peer.slot); }
		}
	}

	bool ServerWorld::ApplyWorldState()
	{
		if (worldStateApplied) { return true; }
		if (scene == nullptr) { return false; }

		if (worldStore != nullptr)
		{
			World::WorldState state;
			if (!worldStore->ReadState(state)) { return false; }

			std::vector<std::uint64_t> unknown;
			scene->FullWipe();
			scene->stations.ApplyWorldState(state.completedStations, &unknown);
			for (const std::uint64_t id : unknown)
			{
				std::cerr << "[Server] World state names station " << id << ", which this World save doesn't have; dropped\n";
			}
			scene->stations.WriteToMap(scene->gameMap);
			std::cout << "[Server] World state applied: " << state.completedStations.size() - unknown.size() << " station(s) completed\n";
		}

		worldStateApplied = true;
		return true;
	}

	// ---- The tick -----------------------------------------------------------

	void ServerWorld::Tick(float deltaTime)
	{
		++serverTick;
		for (Peer& peer : peers) { peer.messagesThisTick = 0; }

		PollTransport();
		CloseTimedOutHellos();

		if (scene != nullptr)
		{
			const std::array<PlayerCommand, kMaxPlayers> commands = TakeCommands();
			SceneManagement::Scene_tick(*scene, deltaTime, std::span<const PlayerCommand>(commands.data(), commands.size()));
			SendEvents();
			if (serverTick % static_cast<std::uint32_t>(config.snapshotEveryTicks) == 0) { SendSnapshots(); }
		}

		// Forget connections closed this tick
		std::erase_if(peers, [](const Peer& peer) { return peer.closing; });
	}

	void ServerWorld::PollTransport()
	{
		std::array<Net::NetEvent, 64> events;
		for (;;)
		{
			const std::size_t count = transport.Poll(events);
			for (std::size_t i = 0; i < count; ++i)
			{
				const Net::NetEvent& event = events[i];
				switch (event.type)
				{
				case Net::NetEventType::Connected:    OnConnected(event.connection); break;
				case Net::NetEventType::Disconnected: OnDisconnected(event.connection); break;
				case Net::NetEventType::Message:
					if (Peer* peer = FindPeer(event.connection); peer != nullptr && !peer->closing)
					{
						// A connection sending more than its budget in one tick is closed (P-7)
						if (++peer->messagesThisTick > config.pollBudgetPerConnection) { Close(*peer, "over its message budget"); break; }
						OnMessage(*peer, event.payload);
					}
					break;
				}
			}
			if (count < events.size()) { break; }
		}
	}

	void ServerWorld::OnConnected(Net::ConnectionId connection)
	{
		if (FindPeer(connection) != nullptr) { return; }

		// Unwelcomed connections are capped at maxPlayers (P-5)
		const auto unwelcomed = std::count_if(peers.begin(), peers.end(), [](const Peer& p) { return !p.welcomed && !p.closing; });
		if (unwelcomed >= config.maxPlayers)
		{
			lastCloseReason = "too many connections waiting for Hello";
			transport.Disconnect(connection);
			return;
		}

		Peer peer;
		peer.connection = connection;
		peer.connectedTick = serverTick;
		peers.push_back(peer);
		std::cout << "[Server] Connection " << connection << " opened\n";
	}

	void ServerWorld::OnDisconnected(Net::ConnectionId connection)
	{
		Peer* peer = FindPeer(connection);
		if (peer == nullptr) { return; }
		std::cout << "[Server] Connection " << connection << " (Slot " << peer->slot << ") went away\n";

		if (peer->welcomed && scene != nullptr)
		{
			// Whatever it held is free again (S-4); its player leaves with it. Slot 0
			// is the scene's own player and stays.
			scene->ReleaseStationsHeldBy(peer->slot, true);
			if (peer->slot != 0) { scene->RemovePlayer(peer->slot); }
		}
		peer->closing = true;
	}

	void ServerWorld::OnMessage(Peer& peer, std::span<const std::byte> bytes)
	{
		Net::Message message;
		Net::Header header;
		const Net::DecodeError error = decoder.Decode(bytes, limits, message, &header);

		if (error == Net::DecodeError::VersionMismatch)
		{
			// R5: a version check first; a Hello from another version gets a Reject it can read
			if (!peer.welcomed && header.type == Net::MsgType::Hello) { Reject(peer, Net::RejectReason::VersionMismatch); }
			else { Close(peer, "a message from another protocol version"); }
			return;
		}
		if (error != Net::DecodeError::None)
		{
			Close(peer, std::string("an undecodable message (") + Net::DecodeErrorName(error) + ")");
			return;
		}

		// Only Hello before Welcome; a second Hello is ignored (P-5)
		if (!peer.welcomed)
		{
			if (const auto* hello = std::get_if<Net::HelloMsg>(&message)) { OnHello(peer, *hello); }
			return;
		}

		if (const auto* commands = std::get_if<Net::CommandsMsg>(&message)) { OnCommands(peer, *commands); }
		else if (const auto* request = std::get_if<Net::StationRequestMsg>(&message)) { OnStationRequest(peer, *request); }
		// Anything else a client has no business sending: ignored
	}

	void ServerWorld::OnHello(Peer& peer, const Net::HelloMsg& hello)
	{
		if (hello.worldHash != worldHash) { Reject(peer, Net::RejectReason::WorldMismatch); return; }

		const int slot = FreeSlot();
		if (slot < 0) { Reject(peer, Net::RejectReason::ServerFull); return; }

		peer.slot = slot;
		peer.welcomed = true;
		peer.name = hello.name;
		if (scene != nullptr) { scene->AddPlayer(slot); }

		Net::WelcomeMsg welcome;
		welcome.slot = slot;
		welcome.tickRate = static_cast<int>(kTickRate);
		welcome.maxPlayers = config.maxPlayers;
		Send(peer, welcome, Net::Delivery::Reliable);
		std::cout << "[Server] Welcomed \"" << hello.name << "\" as Slot " << slot << "\n";
	}

	void ServerWorld::OnCommands(Peer& peer, const Net::CommandsMsg& message)
	{
		// The last few commands, oldest first. Anything newer than what was already
		// accepted is queued; a repeat of one already seen is ignored (P-6).
		for (int i = 0; i < message.count; ++i)
		{
			const PlayerCommand& command = message.commands[static_cast<std::size_t>(i)];
			if (command.sequence <= peer.lastSeq) { continue; }
			peer.lastSeq = command.sequence;
			peer.queue.push_back(command);
			while (static_cast<int>(peer.queue.size()) > config.commandQueueCap) { peer.queue.pop_front(); }
		}
	}

	void ServerWorld::OnStationRequest(Peer& peer, const Net::StationRequestMsg& request)
	{
		// At most a few station requests a second (S-4); a Slot over that goes
		const std::uint32_t window = static_cast<std::uint32_t>(kTickRate);
		if (serverTick - peer.requestWindowTick >= window)
		{
			peer.requestWindowTick = serverTick;
			peer.requestsInWindow = 0;
		}
		if (++peer.requestsInWindow > config.stationRequestsPerSecond)
		{
			Close(peer, "too many station requests");
			return;
		}
		if (scene == nullptr) { return; }

		// The Slot is the connection's, never the message's (P-5)
		switch (request.action)
		{
		case StationAction::Claim:    scene->ClaimStation(request.stationIndex, peer.slot); break;
		case StationAction::Release:  scene->ReleaseStation(request.stationIndex, peer.slot); break;
		case StationAction::Complete: scene->CompleteStation(request.stationIndex, peer.slot); break;
		default: break;
		}
	}

	void ServerWorld::CloseTimedOutHellos()
	{
		for (Peer& peer : peers)
		{
			if (!peer.welcomed && !peer.closing && serverTick - peer.connectedTick > static_cast<std::uint32_t>(config.helloTimeoutTicks))
			{
				Close(peer, "no Hello within 5 s");
			}
		}
	}

	std::array<PlayerCommand, kMaxPlayers> ServerWorld::TakeCommands()
	{
		std::array<PlayerCommand, kMaxPlayers> commands = {};
		for (Peer& peer : peers)
		{
			if (!peer.welcomed || peer.closing || !IsValidSlot(peer.slot)) { continue; }

			// Exactly one command per tick, which is what keeps a flood from speeding
			// anyone up. With none waiting, the last one's movement and held buttons
			// carry on, without its presses or look (P-6).
			PlayerCommand command;
			if (!peer.queue.empty())
			{
				command = peer.queue.front();
				peer.queue.pop_front();
				peer.appliedSeq = command.sequence;
				peer.lastApplied = command;
			}
			else
			{
				command = RepeatOf(peer.lastApplied);
			}
			command.buttons = static_cast<std::uint16_t>(command.buttons & kAllButtons);
			commands[static_cast<std::size_t>(peer.slot)] = command;
		}
		return commands;
	}

	// ---- What goes out ------------------------------------------------------

	void ServerWorld::SendEvents()
	{
		for (const GameEvent& event : scene->TakeEvents())
		{
			switch (event.type)
			{
			case GameEventType::PlayerCaught:
				if (IsValidSlot(event.slot)) { Broadcast(Net::PlayerCaughtMsg{ event.slot }, Net::Delivery::Reliable); }
				break;

			case GameEventType::RoundOver:
				Broadcast(Net::RoundOverMsg{ event.outcome, event.roundEpoch }, Net::Delivery::Reliable);
				// The Full wipe already ran in the tick (C-2); its World state follows it
				if (event.outcome == RoundOutcome::Caught && worldStore != nullptr) { worldStore->SubmitFullWipe(); }
				break;

			case GameEventType::StationEvent:
			{
				if (!IsValidSlot(event.slot) || !scene->stations.IsValidIndex(event.stationIndex)) { break; }
				Net::StationEventMsg message;
				message.kind = event.stationEvent;
				message.stationIndex = event.stationIndex;
				message.slot = event.slot;
				Broadcast(message, Net::Delivery::Reliable);

				// Completion is the one fact the World store keeps (Q48)
				if (event.stationEvent == StationEventKind::Completed && worldStore != nullptr)
				{
					worldStore->SubmitStationCompleted(scene->stations.At(event.stationIndex).id);
				}
				break;
			}
			}
		}
	}

	void ServerWorld::SendSnapshots()
	{
		Net::SnapshotMsg snapshot;
		snapshot.serverTick = serverTick;
		snapshot.roundEpoch = scene->roundEpoch;

		scene->ForEachPlayer([&](int slot, Player& player) {
			if (snapshot.playerCount >= kMaxPlayers) { return; }
			Net::SnapshotPlayer& p = snapshot.players[static_cast<std::size_t>(snapshot.playerCount++)];
			const Vector3 at = player.getPosition();
			p.slot = slot;
			p.x = at.x; p.y = at.y; p.z = at.z;
			p.yaw = player.lookYaw;
			p.pitch = player.lookPitch;
			p.flashlight = player.flashlightOn;
			p.crouching = player.isCrouching;
		});

		scene->gameMap.ForEachEntity([&](Entity& entity) {
			if (snapshot.hasStalker || entity.kind != ENTITYKIND_STALKER || !entity.isEnabled) { return; }
			const Stalker& stalker = static_cast<const Stalker&>(entity);
			const Vector3 at = stalker.getPosition();
			const Vector3 velocity = stalker.getVelocity();
			const float planarSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
			snapshot.hasStalker = true;
			snapshot.stalker.x = at.x; snapshot.stalker.y = at.y; snapshot.stalker.z = at.z;
			snapshot.stalker.yaw = std::atan2(stalker.rigidBody3D.forward.x, stalker.rigidBody3D.forward.z);
			snapshot.stalker.state = static_cast<int>(stalker.state);
			snapshot.stalker.gait = planarSpeed < 0.1f ? Net::StalkerGait::Still
				: stalker.isSprinting ? Net::StalkerGait::Sprint : Net::StalkerGait::Walk;
		});

		snapshot.stationCount = scene->stations.Count();
		for (int i = 0; i < snapshot.stationCount; ++i)
		{
			const StationEntry& entry = scene->stations.At(i);
			snapshot.stations[static_cast<std::size_t>(i)].completed = entry.Completed();
			snapshot.stations[static_cast<std::size_t>(i)].holder = entry.holder;
		}

		for (const Peer& peer : peers)
		{
			if (!peer.welcomed || peer.closing) { continue; }
			snapshot.ackSeq = peer.appliedSeq;
			const std::span<const std::byte> bytes = encoder.Encode(snapshot, limits);
			if (encoder.FloatsReplaced() > 0) { std::cerr << "[Server] " << encoder.FloatsReplaced() << " non-finite float(s) in a Snapshot replaced with 0\n"; }
			if (!bytes.empty()) { transport.Send(peer.connection, bytes, Net::Delivery::Unreliable); }
		}
	}

	void ServerWorld::Send(const Peer& peer, const Net::Message& message, Net::Delivery delivery)
	{
		const std::span<const std::byte> bytes = encoder.Encode(message, limits);
		if (bytes.empty())
		{
			std::cerr << "[Server] Refused to encode a message of type " << static_cast<int>(Net::TypeOf(message)) << "\n";
			return;
		}
		transport.Send(peer.connection, bytes, delivery);
	}

	void ServerWorld::Broadcast(const Net::Message& message, Net::Delivery delivery)
	{
		const std::span<const std::byte> bytes = encoder.Encode(message, limits);
		if (bytes.empty()) { return; }
		for (const Peer& peer : peers)
		{
			if (peer.welcomed && !peer.closing) { transport.Send(peer.connection, bytes, delivery); }
		}
	}

	void ServerWorld::Reject(Peer& peer, Net::RejectReason reason)
	{
		std::cout << "[Server] Rejecting connection " << peer.connection << ": " << Net::RejectReasonName(reason) << "\n";
		Send(peer, Net::RejectMsg{ reason }, Net::Delivery::Reliable);
		// Closed with linger (GnsTransport), so the Reject still reaches the client
		Close(peer, std::string("rejected: ") + Net::RejectReasonName(reason));
	}

	void ServerWorld::Close(Peer& peer, const std::string& reason)
	{
		if (peer.closing) { return; }
		std::cout << "[Server] Closing connection " << peer.connection << ": " << reason << "\n";
		lastCloseReason = reason;
		if (peer.welcomed && scene != nullptr)
		{
			scene->ReleaseStationsHeldBy(peer.slot, true);
			if (peer.slot != 0) { scene->RemovePlayer(peer.slot); }
		}
		transport.Disconnect(peer.connection);
		peer.closing = true;
	}

	void ServerWorld::DisconnectAll()
	{
		for (Peer& peer : peers) { if (!peer.closing) { Close(peer, "the server is shutting down"); } }
		peers.clear();
	}

	// ---- Lookups ------------------------------------------------------------

	ServerWorld::Peer* ServerWorld::FindPeer(Net::ConnectionId connection)
	{
		for (Peer& peer : peers) { if (peer.connection == connection && !peer.closing) { return &peer; } }
		return nullptr;
	}

	ServerWorld::Peer* ServerWorld::PeerInSlot(int slot)
	{
		for (Peer& peer : peers) { if (peer.welcomed && !peer.closing && peer.slot == slot) { return &peer; } }
		return nullptr;
	}

	int ServerWorld::FreeSlot() const
	{
		for (int slot = 0; slot < config.maxPlayers; ++slot)
		{
			const bool taken = std::any_of(peers.begin(), peers.end(), [&](const Peer& p) { return p.welcomed && !p.closing && p.slot == slot; });
			if (!taken) { return slot; }
		}
		return -1;
	}

	std::vector<ServerWorld::PeerInfo> ServerWorld::Peers() const
	{
		std::vector<PeerInfo> info;
		for (const Peer& peer : peers)
		{
			if (!peer.closing) { info.push_back({ peer.connection, peer.slot, peer.welcomed, peer.name }); }
		}
		return info;
	}

	int ServerWorld::WelcomedCount() const
	{
		return static_cast<int>(std::count_if(peers.begin(), peers.end(), [](const Peer& p) { return p.welcomed && !p.closing; }));
	}

	void ServerWorld::ReleaseStationsHeldBy(int slot, bool notify)
	{
		if (scene != nullptr) { scene->ReleaseStationsHeldBy(slot, notify); }
	}
}
