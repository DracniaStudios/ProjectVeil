#include "ClientWorld.h"

#include <Scene.h>
#include <AI/Stalker.h>
#include <Client/LocalPlayer.h>
#include <Client/RoundPresenter.h>
#include <sim/FixedTick.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace Client {

	namespace {
		double SteadyNow()
		{
			using namespace std::chrono;
			return duration<double>(steady_clock::now().time_since_epoch()).count();
		}

		float Lerp(float a, float b, float t) { return a + (b - a) * t; }

		float LerpAngle(float a, float b, float t)
		{
			constexpr float kTwoPi = 6.28318530718f;
			float d = std::fmod(b - a, kTwoPi);
			if (d > kTwoPi / 2) { d -= kTwoPi; }
			if (d < -kTwoPi / 2) { d += kTwoPi; }
			return a + d * t;
		}

		Stalker* FindStalker(SceneManagement::Scene& scene)
		{
			Stalker* found = nullptr;
			scene.gameMap.ForEachEntity([&](Entity& entity) {
				if (entity.kind == ENTITYKIND_STALKER) { found = static_cast<Stalker*>(&entity); return false; }
				return true;
			});
			return found;
		}
	}

	const char* ConnectionStateName(ConnectionState state)
	{
		switch (state)
		{
		case ConnectionState::Idle:       return "Idle";
		case ConnectionState::Connecting: return "Connecting";
		case ConnectionState::Welcomed:   return "Connected";
		case ConnectionState::Rejected:   return "Rejected";
		case ConnectionState::Lost:       return "Disconnected";
		}
		return "Unknown";
	}

	ClientWorld::ClientWorld(Net::INetTransport& transport, bool remote) : transport(transport), remote(remote) {}

	bool ClientWorld::Connect(std::string_view address, std::uint64_t hash, std::string playerName, int stationCount)
	{
		worldHash = hash;
		name = Net::IsValidName(playerName) ? std::move(playerName) : std::string("Player");
		limits.stationCount = stationCount;
		connection = transport.Connect(address);
		state = connection == Net::kInvalidConnection ? ConnectionState::Lost : ConnectionState::Connecting;
		return connection != Net::kInvalidConnection;
	}

	void ClientWorld::Send(const Net::Message& message, Net::Delivery delivery)
	{
		const std::span<const std::byte> bytes = encoder.Encode(message, limits);
		if (!bytes.empty()) { transport.Send(connection, bytes, delivery); }
	}

	void ClientWorld::SendCommand(const Sim::PlayerCommand& command)
	{
		// The last three, oldest first
		if (recentCount < Net::kMaxCommandsPerMessage) { recent[static_cast<std::size_t>(recentCount++)] = command; }
		else
		{
			std::rotate(recent.begin(), recent.begin() + 1, recent.end());
			recent.back() = command;
		}

		// A remote client turns its own view at once; the server reaches the same
		// angles from the same deltas (clamped the way Player::ApplyLook clamps)
		if (remote)
		{
			constexpr float kHalfTurn = 3.14159265358979323846f;
			const auto clampDelta = [kHalfTurn](float d) { return std::isfinite(d) ? std::clamp(d, -kHalfTurn, kHalfTurn) : 0.0f; };
			predictedYaw = std::remainder(predictedYaw + clampDelta(command.yawDelta), 2.0f * kHalfTurn);
			predictedPitch = std::clamp(predictedPitch + clampDelta(command.pitchDelta), -Player::kPitchLimit, Player::kPitchLimit);
		}

		if (state != ConnectionState::Welcomed) { return; }
		Net::CommandsMsg message;
		message.count = recentCount;
		message.commands = recent;
		Send(message, Net::Delivery::Unreliable);
	}

	void ClientWorld::SendStationRequests(SceneManagement::Scene& scene)
	{
		for (const Sim::StationRequest& request : scene.TakeStationRequests())
		{
			if (state != ConnectionState::Welcomed) { continue; }
			Send(Net::StationRequestMsg{ request.action, request.stationIndex }, Net::Delivery::Reliable);
		}
	}

	void ClientWorld::Poll(SceneManagement::Scene* scene)
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
				case Net::NetEventType::Connected:
				{
					// Hello first, and only Hello, until Welcome (P-5)
					Net::HelloMsg hello;
					hello.worldHash = worldHash;
					hello.name = name;
					Send(hello, Net::Delivery::Reliable);
					break;
				}
				case Net::NetEventType::Disconnected:
					if (state != ConnectionState::Rejected) { state = ConnectionState::Lost; }
					std::cout << "[Client] Disconnected\n";
					break;
				case Net::NetEventType::Message:
					OnMessage(scene, event.payload);
					break;
				}
			}
			if (count < events.size()) { break; }
		}
	}

	void ClientWorld::OnMessage(SceneManagement::Scene* scene, std::span<const std::byte> bytes)
	{
		Net::Message message;
		const Net::DecodeError error = decoder.Decode(bytes, limits, message);
		if (error != Net::DecodeError::None)
		{
			std::cerr << "[Client] Dropped a message: " << Net::DecodeErrorName(error) << "\n";
			return;
		}

		if (const auto* welcome = std::get_if<Net::WelcomeMsg>(&message))
		{
			// P-8: slot < maxPlayers <= 16, tickRate > 0
			if (state != ConnectionState::Connecting || welcome->slot < 0 || welcome->slot >= welcome->maxPlayers
				|| welcome->maxPlayers > Sim::kMaxPlayers || welcome->tickRate <= 0)
			{
				std::cerr << "[Client] Ignored a Welcome that breaks the rules\n";
				return;
			}
			slot = welcome->slot;
			maxPlayers = welcome->maxPlayers;
			state = ConnectionState::Welcomed;
			SetLocalSlot(slot);
			std::cout << "[Client] Welcomed as Slot " << slot << " of " << maxPlayers << "\n";
			return;
		}

		if (const auto* reject = std::get_if<Net::RejectMsg>(&message))
		{
			rejectReason = reject->reason;
			state = ConnectionState::Rejected;
			std::cout << "[Client] Rejected: " << Net::RejectReasonName(reject->reason) << "\n";
			return;
		}

		if (state != ConnectionState::Welcomed || scene == nullptr) { return; }

		if (const auto* snapshot = std::get_if<Net::SnapshotMsg>(&message)) { OnSnapshot(scene, *snapshot); return; }

		if (const auto* caught = std::get_if<Net::PlayerCaughtMsg>(&message))
		{
			if (caught->slot >= maxPlayers) { return; }
			Sim::GameEvent event;
			event.type = Sim::GameEventType::PlayerCaught;
			event.slot = caught->slot;
			PresentEvent(*scene, event);
			return;
		}

		if (const auto* over = std::get_if<Net::RoundOverMsg>(&message))
		{
			Sim::GameEvent event;
			event.type = Sim::GameEventType::RoundOver;
			event.outcome = over->outcome;
			event.roundEpoch = over->roundEpoch;
			EndRoundNow(scene, over->roundEpoch + 1);
			PresentEvent(*scene, event);
			return;
		}

		if (const auto* station = std::get_if<Net::StationEventMsg>(&message))
		{
			if (station->slot >= maxPlayers) { return; }
			Sim::GameEvent event;
			event.type = Sim::GameEventType::StationEvent;
			event.stationEvent = station->kind;
			event.stationIndex = station->stationIndex;
			event.slot = station->slot;
			PresentEvent(*scene, event);
			return;
		}
	}

	void ClientWorld::EndRoundNow(SceneManagement::Scene* scene, std::uint32_t epoch)
	{
		if (haveEpoch && epoch <= knownEpoch) { return; }
		const bool first = !haveEpoch;
		haveEpoch = true;
		knownEpoch = epoch;
		if (first || scene == nullptr) { return; }

		// P-8: a new epoch ends the round at once, even before RoundOver arrives:
		// the MiniGame closes and nothing asked of the old round is sent
		scene->QuitMiniGame();
		scene->stationRequests.clear();
		lookSeeded = false; // the wipe put the look back; take it from the next Snapshot
	}

	void ClientWorld::OnSnapshot(SceneManagement::Scene* scene, const Net::SnapshotMsg& snapshot)
	{
		// P-8: never an older or repeated Snapshot; serverTick never resets
		if (snapshot.serverTick <= newestServerTick)
		{
			++droppedSnapshots;
			return;
		}
		// Every count and slot against this server's maxPlayers
		if (snapshot.playerCount > maxPlayers)
		{
			++droppedSnapshots;
			return;
		}
		for (int i = 0; i < snapshot.playerCount; ++i)
		{
			if (snapshot.players[static_cast<std::size_t>(i)].slot >= maxPlayers) { ++droppedSnapshots; return; }
		}
		for (int i = 0; i < snapshot.stationCount; ++i)
		{
			if (snapshot.stations[static_cast<std::size_t>(i)].holder >= maxPlayers) { ++droppedSnapshots; return; }
		}

		newestServerTick = snapshot.serverTick;
		ackedSequence = snapshot.ackSeq;
		newestSnapshotAt = SteadyNow();
		EndRoundNow(scene, snapshot.roundEpoch);

		if (!remote) { return; }

		Received received;
		received.snapshot = snapshot;
		received.serverTime = static_cast<double>(snapshot.serverTick) / Sim::kTickRate;

		// The server's clock as seen from here: the smallest offset is the one
		// with the least network delay in it
		const double offset = newestSnapshotAt - received.serverTime;
		if (!haveClock || offset < clockOffset) { clockOffset = offset; haveClock = true; }
		else { clockOffset += (offset - clockOffset) * 0.01; } // drift follows slowly

		snapshots.push_back(received);
		while (snapshots.size() > 32) { snapshots.pop_front(); }

		if (!lookSeeded)
		{
			for (int i = 0; i < snapshot.playerCount; ++i)
			{
				const Net::SnapshotPlayer& p = snapshot.players[static_cast<std::size_t>(i)];
				if (p.slot == slot) { predictedYaw = p.yaw; predictedPitch = p.pitch; lookSeeded = true; }
			}
		}
	}

	void ClientWorld::ApplySnapshots(SceneManagement::Scene& scene, double now)
	{
		if (!remote || snapshots.empty()) { return; }
		if (now <= 0.0) { now = SteadyNow(); }
		lastApplyNow = now;

		const Received& newest = snapshots.back();
		const double renderTime = now - clockOffset - kInterpolationDelay;

		// The two Snapshots around the render time; never across a round's end
		const Received* from = &snapshots.front();
		const Received* to = &snapshots.front();
		for (std::size_t i = 0; i + 1 < snapshots.size(); ++i)
		{
			if (snapshots[i].serverTime <= renderTime && renderTime <= snapshots[i + 1].serverTime)
			{
				from = &snapshots[i];
				to = &snapshots[i + 1];
				break;
			}
			if (snapshots[i + 1].serverTime <= renderTime) { from = to = &snapshots[i + 1]; }
		}
		if (from->snapshot.roundEpoch != to->snapshot.roundEpoch) { from = to; }
		if (to->snapshot.roundEpoch != newest.snapshot.roundEpoch) { from = to = &newest; }

		const double span = to->serverTime - from->serverTime;
		const float t = span > 0.0 ? static_cast<float>(std::clamp((renderTime - from->serverTime) / span, 0.0, 1.0)) : 1.0f;

		// Players: everyone in the newest Snapshot has a player; nobody else does
		bool present[Sim::kMaxPlayers] = {};
		for (int i = 0; i < newest.snapshot.playerCount; ++i) { present[newest.snapshot.players[static_cast<std::size_t>(i)].slot] = true; }
		for (int s = 0; s < Sim::kMaxPlayers; ++s)
		{
			if (!present[s] && s != 0 && scene.PlayerInSlot(s) != nullptr) { scene.RemovePlayer(s); }
		}

		const auto findIn = [](const Net::SnapshotMsg& snap, int s) -> const Net::SnapshotPlayer* {
			for (int i = 0; i < snap.playerCount; ++i) { if (snap.players[static_cast<std::size_t>(i)].slot == s) { return &snap.players[static_cast<std::size_t>(i)]; } }
			return nullptr;
		};

		for (int i = 0; i < newest.snapshot.playerCount; ++i)
		{
			const Net::SnapshotPlayer& latest = newest.snapshot.players[static_cast<std::size_t>(i)];
			Player* player = scene.AddPlayer(latest.slot);
			if (player == nullptr) { continue; }

			// The local player has no prediction: it stands where the server last
			// said. Everyone else is drawn 100 ms behind, between two Snapshots.
			const Net::SnapshotPlayer* a = latest.slot == slot ? &latest : findIn(from->snapshot, latest.slot);
			const Net::SnapshotPlayer* b = latest.slot == slot ? &latest : findIn(to->snapshot, latest.slot);
			if (a == nullptr || b == nullptr) { a = b = &latest; }

			const Vector3 at = { Lerp(a->x, b->x, t), Lerp(a->y, b->y, t), Lerp(a->z, b->z, t) };
			player->rigidBody3D.Teleport(at);
			player->rigidBody3D.SetVelocity(Vector3Zero());
			player->flashlightOn = b->flashlight;
			player->isCrouching = b->crouching;
			player->rigidBody3D.scale = Vector3(1, 2, 1);

			if (latest.slot == slot)
			{
				player->SetLookAngles(predictedYaw, predictedPitch);
			}
			else
			{
				player->SetLookAngles(LerpAngle(a->yaw, b->yaw, t), Lerp(a->pitch, b->pitch, t));
			}
			player->camera.position = player->EyePosition();
		}

		// The stalker
		if (Stalker* stalker = FindStalker(scene))
		{
			if (to->snapshot.hasStalker && from->snapshot.hasStalker)
			{
				const Net::SnapshotStalker& a = from->snapshot.stalker;
				const Net::SnapshotStalker& b = to->snapshot.stalker;
				stalker->rigidBody3D.Teleport(Vector3{ Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t) });
				stalker->rigidBody3D.SetVelocity(Vector3Zero());
				const float yaw = LerpAngle(a.yaw, b.yaw, t);
				stalker->rigidBody3D.forward = Vector3{ std::sin(yaw), 0.0f, std::cos(yaw) };
				stalker->state = static_cast<StalkerState>(b.state);
				stalker->isSprinting = b.gait == Net::StalkerGait::Sprint;
				stalker->isEnabled = true;
			}
		}

		// Stations: completion and holders from the newest Snapshot. Targets are
		// derived from them here, never toggled by a StationEvent (S-6).
		bool changed = false;
		for (int i = 0; i < newest.snapshot.stationCount && i < scene.stations.Count(); ++i)
		{
			Sim::StationEntry& entry = scene.stations.At(i);
			const Net::SnapshotStation& s = newest.snapshot.stations[static_cast<std::size_t>(i)];
			const bool inWorld = s.completed && !entry.authoredCompleted;
			if (entry.completedInWorld != inWorld || entry.holder != s.holder)
			{
				entry.completedInWorld = inWorld;
				entry.holder = s.holder;
				changed = true;
			}
		}
		if (changed) { scene.stations.WriteToMap(scene.gameMap); }
	}
}
