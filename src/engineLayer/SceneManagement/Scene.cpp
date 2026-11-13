#include "Scene.h"

#include <Geometry.h>
#include <ContactRules.h>
#include <AI/Stalker.h>
#include <SceneManager.h>
#include <World/WorldKey.h>
#include <sim/FixedTick.h>
#include <sim/Presentation.h>
#include <net/Protocol.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>

namespace SceneManagement {

	Scene* Scene_new() {
		Scene* scene = new Scene;
		scene->gameMap = {};

		// Slot 0: the single-player player, and the Host's own
		scene->AddPlayer(0);

		// Not made current here: SceneManager does that once the scene is fully
		// built, and everything that builds it is handed the scene itself
		return scene;
	}

	void Scene_delete(Scene* scene) {
		if (scene == nullptr) { return; }

		Sim::CurrentPresentation().OnSceneDeleting(*scene);

		// Releases every generated model, which ~GameObject() never would
		scene->gameMap.Clear();

		for (int slot = 0; slot < Sim::kMaxPlayers; ++slot) { scene->RemovePlayer(slot); }

		delete scene;
	}

#pragma region Players

	Player* Scene::AddPlayer(int slot)
	{
		if (!Sim::IsValidSlot(slot)) { return nullptr; }
		if (players[slot] != nullptr) { return players[slot].get(); }

		players[slot] = std::make_unique<Player>();
		Player* player = players[slot].get();
		player->type = OBJECT_PLAYER;
		player->id = Sim::PlayerIdForSlot(slot);
		player->setSpawnPoint(SpawnPointFor(slot));
		player->rigidBody3D.Teleport(player->getSpawnPoint());
		player->onEnable();

		return player;
	}

	void Scene::RemovePlayer(int slot)
	{
		Player* player = PlayerInSlot(slot);
		if (player == nullptr) { return; }

		// The artifact is created on first use (InteractableObject.cpp) and owned by the player.
		if (player->artifact) {
			player->artifact->releaseGeneratedModel();
		}
		player->releaseGeneratedModel();
		players[slot].reset();
	}

	Player* Scene::FindPlayer(std::uint64_t id) const
	{
		const int slot = Sim::SlotForPlayerId(id);
		Player* player = PlayerInSlot(slot);
		return (player != nullptr && player->id == id) ? player : nullptr;
	}

	int Scene::PlayerCount() const
	{
		int count = 0;
		for (const auto& player : players) { if (player) { ++count; } }
		return count;
	}

	Vector3 Scene::SpawnPointFor(int slot) const
	{
		// One body width and a half apart, along +X
		constexpr float kSlotSpacing = 1.5f;
		return Vector3Add(spawnPoint, Vector3{ kSlotSpacing * static_cast<float>(slot), 0.0f, 0.0f });
	}

	void Scene::SetSpawnPoint(Vector3 point)
	{
		spawnPoint = point;
		ForEachPlayer([&](int slot, Player& player) {
			player.setSpawnPoint(SpawnPointFor(slot));
			player.rigidBody3D.Teleport(player.getSpawnPoint());
		});
	}
#pragma endregion



	void Scene::ClearWorld() {
		gameMap.Clear();
		physics.Reset();
	}

#pragma region Physics Management

	/** Physics Solutions **/
	std::vector<Physics3D::ContactEvent> Scene::SolveCollisions(int iterations)
	{
		// Every body in the scene in one list, so every pair follows the same rules
		// whatever kind of object owns each body
		std::vector<Physics3D::PhysicsBody> bodies;
		bodies.reserve(gameMap.GameObjectCount() + gameMap.EntityCount() + gameMap.InteractableCount() + Sim::kMaxPlayers);
		gameMap.ForEachObject([&](GameObject& object) { bodies.push_back({ object.id, &object.rigidBody3D }); });
		ForEachPlayer([&](int, Player& player) { bodies.push_back({ player.id, &player.rigidBody3D }); });

		const std::vector<Physics3D::ContactEvent> events = physics.Solve(bodies, iterations);
		ContactRules::Apply(*this, events);
		return events;
	}
#pragma endregion

#pragma region Catches

	void Scene::RaiseCatch(std::uint64_t playerId)
	{
		if (std::find(caughtThisTick.begin(), caughtThisTick.end(), playerId) != caughtThisTick.end()) { return; }
		caughtThisTick.push_back(playerId);
	}

	void Scene::CheckCatches()
	{
		// How far apart in height a stalker and a player can be and still count as
		// close: a player on a ledge above the stalker is out of reach
		constexpr float kCatchHeight = 1.5f;

		gameMap.ForEachStalker([&](Stalker& stalker) {
			if (!stalker.isEnabled) { return; }

			const auto check = [&](Player& candidate) {
				if (!candidate.isEnabled) { return; }

				const Vector3 a = stalker.getPosition();
				const Vector3 b = candidate.getPosition();
				const float planar = Vector2Distance(Vector2{ a.x, a.z }, Vector2{ b.x, b.z });
				const bool inReach = planar <= stalker.catchRadius && std::fabs(a.y - b.y) <= kCatchHeight;

				if (inReach || physics.IsTouching(stalker.id, candidate.id)) { RaiseCatch(candidate.id); }
			};
			ForEachPlayer([&](int, Player& player) { check(player); });
		});
	}

	void Scene::CheckEscapes()
	{
		gameMap.ForEachInteractable([&](InteractableObject& exit) {
			if (exit.interactType != INTERACT_EXIT || !exit.IsExitUnlocked(gameMap)) { return; }

			const BoundingBox volume = exit.rigidBody3D.broadPhaseBox;
			ForEachPlayer([&](int, Player& player) {
				if (!player.isEnabled) { return; }
				const bool inside = Geometry::CheckCollisionBoxes(volume, player.rigidBody3D.broadPhaseBox)
					|| physics.IsTouching(exit.id, player.id);
				if (inside && std::find(escapedThisTick.begin(), escapedThisTick.end(), player.id) == escapedThisTick.end())
				{
					escapedThisTick.push_back(player.id);
				}
			});
		});
	}

	void Scene::ResolveRound()
	{
		if (caughtThisTick.empty() && escapedThisTick.empty()) { return; }

		// A round already won counts nothing more until the scene is rebuilt
		if (roundWon)
		{
			caughtThisTick.clear();
			escapedThisTick.clear();
			return;
		}

		Sim::RoundState state = {};
		state.playersInRound = PlayerCount();
		state.playersCaught = static_cast<int>(caughtThisTick.size());
		state.playersEscaped = static_cast<int>(escapedThisTick.size());

		// Every catch is reported, even when one of them is enough to end the round
		for (const std::uint64_t id : caughtThisTick)
		{
			Sim::GameEvent caught = {};
			caught.type = Sim::GameEventType::PlayerCaught;
			caught.slot = Sim::SlotForPlayerId(id);
			caught.roundEpoch = roundEpoch;
			outbox.push_back(caught);
			std::cout << "[Round] Slot " << caught.slot << " caught\n";
		}
		caughtThisTick.clear();
		escapedThisTick.clear();

		const Sim::RoundOutcome outcome = Sim::EvaluateRound(state);
		if (outcome == Sim::RoundOutcome::Ongoing) { return; }

		Sim::GameEvent over = {};
		over.type = Sim::GameEventType::RoundOver;
		over.outcome = outcome;
		over.roundEpoch = roundEpoch;
		outbox.push_back(over);
		std::cout << "[Round] Round " << roundEpoch << " over: " << Sim::RoundOutcomeName(outcome) << "\n";

		if (outcome == Sim::RoundOutcome::Caught) { FullWipe(); }
		else if (outcome == Sim::RoundOutcome::Escaped) { roundWon = true; }

		++roundEpoch;
	}

	void Scene::ResetPlayerToSpawn(int slot)
	{
		Player* player = PlayerInSlot(slot);
		if (player == nullptr) { return; }

		// The artifact is what the Unlock pickups built; the pickups come back with
		// the world, so it goes
		if (player->artifact)
		{
			player->artifact->releaseGeneratedModel();
			player->artifact.reset();
		}
		player->artifactUnlocked = -1;
		player->artifactMode = 0;

		player->onEnable(); // full health and stamina, fresh buffs
		player->isEnabled = true;
		player->isCrouching = false;
		player->isSprinting = false;
		player->isFiring = false;
		player->flashlightOn = false;
		player->footstepTimer = 0.0f;
		player->moveDirection = Vector2Zero();

		player->setSpawnPoint(SpawnPointFor(slot));
		player->rigidBody3D.Teleport(player->getSpawnPoint());
		player->rigidBody3D.SetVelocity(Vector3Zero());
		player->SetLookAngles(0.0f, 0.0f);
	}

	void Scene::FullWipe()
	{
		std::cout << "[Round] Full wipe\n";

		// Any MiniGame in progress belongs to the round that just ended
		Sim::CurrentPresentation().OnFullWipe(*this);
		stationNoiseTimer = 0.0f;

		// The world as authored, every object under its own id. Without a World
		// save (a scene built in code) the world stays as it is.
		SaveSystem::RestoreAuthoredObjects(*this);

		// Stalkers go back to Patrol whatever the World save recorded
		gameMap.ForEachStalker([](Stalker& stalker) {
			stalker.state = STALKER_PATROL;
			stalker.timeInState = 0.0f;
			stalker.hasLastKnown = false;
		});

		for (int slot = 0; slot < Sim::kMaxPlayers; ++slot) { ResetPlayerToSpawn(slot); }

		// No station completed through play, no holders: the World state is empty
		stations.Wipe();
		stations.WriteToMap(gameMap);

		director.Clear();
		soundField.Clear();
		caughtThisTick.clear();
		escapedThisTick.clear();
		roundWon = false;
	}
#pragma endregion

#pragma region MiniGame Management

	InteractableObject* Scene::GetRunningStation()
	{
		return gameMap.FindRunningStation();
	}

	// A running task station is a periodic noise source (plan's emitter table:
	// ~0.6, periodic while active).
	void Scene::EmitStationNoise(float deltaTime)
	{
		constexpr float kStationInterval = 0.9f;
		constexpr float kStationLoudness = 0.6f;

		InteractableObject* station = GetRunningStation();
		if (station == nullptr || !station->isEnabled)
		{
			// Reset rather than freeze, so the next station the player starts is
			// audible immediately instead of inheriting a part-spent countdown.
			stationNoiseTimer = 0.0f;
			return;
		}

		stationNoiseTimer -= deltaTime;
		if (stationNoiseTimer > 0.0f) { return; }
		stationNoiseTimer = kStationInterval;

		soundField.Emit(station->getPosition(), kStationLoudness, SOUND_STATION, station->id);
	}

	namespace {
		// What winning each MiniGame grants the player
		struct MiniGameReward
		{
			Buff buff = BUFF_MOVEMENT;
			float heal = 0.0f;
		};

		std::optional<MiniGameReward> RewardFor(int gameId)
		{
			switch (gameId)
			{
			case MINI_GAME_FLAPPY_BIRD_ID: return MiniGameReward{ BUFF_MOVEMENT };
			case MINI_GAME_CRANE_ID:       return MiniGameReward{ BUFF_RANGE };
			case MINI_GAME_DOCTOR_ID:      return MiniGameReward{ BUFF_HEALTH, 3.0f };
			case MINI_GAME_SIMON_SAYS_ID:  return MiniGameReward{ BUFF_HEARING };
			case MINI_GAME_MAZE_ID:        return MiniGameReward{ BUFF_SEARCH };
			case MINI_GAME_RO_SHAM_BOO_ID: return MiniGameReward{ BUFF_RANDOM };
			default:                       return std::nullopt;
			}
		}
	}

	void Scene::OnWorldLoaded()
	{
		if (!stations.Build(gameMap))
		{
			std::cerr << "[Scene] The World save has more Task stations than the protocol carries (" << Net::kMaxStations << "); none will work\n";
		}
		stations.WriteToMap(gameMap);
		contentHash = World::ContentHash(*this);
	}

	int Scene::StationIndexOf(std::uint64_t stationId)
	{
		int index = stations.IndexOf(stationId);
		if (index < 0)
		{
			stations.Build(gameMap);
			index = stations.IndexOf(stationId);
		}
		return index;
	}

	void Scene::ReportStation(Sim::StationEventKind kind, int index, int slot)
	{
		Sim::GameEvent event = {};
		event.type = Sim::GameEventType::StationEvent;
		event.stationEvent = kind;
		event.stationIndex = index;
		event.slot = slot;
		event.roundEpoch = roundEpoch;
		outbox.push_back(event);
	}

	bool Scene::ClaimStation(int index, int slot)
	{
		// How close a player must stand to claim a station (its interact reach,
		// buffed, plus the station's own size)
		constexpr float kClaimReach = 12.0f;

		Player* player = PlayerInSlot(slot);
		const auto deny = [&](const char* why) {
			std::cout << "[Station] Slot " << slot << " denied station " << index << ": " << why << "\n";
			if (stations.IsValidIndex(index)) { ReportStation(Sim::StationEventKind::Denied, index, slot); }
			return false;
		};

		if (!stations.IsValidIndex(index)) { return deny("not in the table"); }
		Sim::StationEntry& entry = stations.At(index);

		InteractableObject* station = gameMap.FindInteractable(entry.id);
		if (station == nullptr || !station->isEnabled) { return deny("the station is gone"); }

		// Starting a station is loud, and so is rattling one that won't start: a
		// silent failed attempt would be the safest way to play (decision 15)
		soundField.Emit(station->getPosition(), 0.8f, SOUND_STATION_ACTIVATION, station->id);

		if (player == nullptr || !player->isEnabled) { return deny("no player in that Slot"); }
		if (entry.holder == slot) { return false; } // asking twice changes nothing
		if (entry.Completed()) { return deny("already completed"); }
		if (entry.InUse()) { return deny("held by another Slot"); }
		if (stations.HeldBy(slot) >= 0) { return deny("the Slot holds another station"); }
		if (std::find(caughtThisTick.begin(), caughtThisTick.end(), player->id) != caughtThisTick.end()) { return deny("the Slot was caught"); }

		const Vector3 a = player->getPosition();
		const Vector3 b = station->getPosition();
		if (Vector2Distance(Vector2{ a.x, a.z }, Vector2{ b.x, b.z }) > kClaimReach) { return deny("too far away"); }

		// The unlock gate, on the server: the artifact tier must reach the game
		if (player->artifactUnlocked < entry.game) { return deny("MiniGame not unlocked"); }

		entry.holder = slot;
		entry.grantedTick = simTick;
		stations.WriteToMap(gameMap);
		stationNoiseTimer = 0.0f;

		ReportStation(Sim::StationEventKind::Granted, index, slot);
		return true;
	}

	bool Scene::CompleteStation(int index, int slot)
	{
		if (!stations.IsValidIndex(index)) { return false; }
		Sim::StationEntry& entry = stations.At(index);

		if (entry.holder != slot)
		{
			std::cout << "[Station] Slot " << slot << " can't complete station " << index << ": it isn't the holder\n";
			ReportStation(Sim::StationEventKind::Denied, index, slot);
			return false;
		}

		// Timed on the server, in ticks, from the grant (S-3)
		const float held = static_cast<float>(simTick - entry.grantedTick) / Sim::kTickRate;
		if (held < entry.minimumSeconds)
		{
			std::cout << "[Station] Slot " << slot << " completed station " << index << " after " << held
				<< " s, under its minimum of " << entry.minimumSeconds << " s: refused\n";
			ReportStation(Sim::StationEventKind::Denied, index, slot);
			return false;
		}

		entry.holder = -1;
		entry.completedInWorld = true;
		stations.WriteToMap(gameMap);

		// The reward is the server's to give (S-5)
		if (Player* player = PlayerInSlot(slot))
		{
			if (const std::optional<MiniGameReward> reward = RewardFor(entry.game))
			{
				if (reward->heal > 0.0f) { player->applyHealthValue(reward->heal, false); }
				if (CooldownTimer* buff = player->getBuff(reward->buff)) { buff->use(); }
			}
		}

		std::cout << "[Station] Slot " << slot << " completed station " << entry.id << "\n";
		ReportStation(Sim::StationEventKind::Completed, index, slot);
		return true;
	}

	bool Scene::ReleaseStation(int index, int slot)
	{
		if (!stations.IsValidIndex(index)) { return false; }
		Sim::StationEntry& entry = stations.At(index);
		if (entry.holder != slot) { return false; }

		entry.holder = -1;
		stations.WriteToMap(gameMap);
		ReportStation(Sim::StationEventKind::Released, index, slot);
		return true;
	}

	void Scene::ReleaseStationsHeldBy(int slot, bool report)
	{
		const int index = stations.HeldBy(slot);
		if (index < 0) { return; }
		stations.At(index).holder = -1;
		stations.WriteToMap(gameMap);
		if (report) { ReportStation(Sim::StationEventKind::Released, index, slot); }
	}
#pragma endregion

#pragma region Scene Update
	/** Scene Functions **/
	void Scene_tick(Scene& scene, float delta, std::span<const Sim::PlayerCommand> commands, bool simulationFrozen) {

		++scene.simTick;

		if (scene.update) { scene.update(scene, delta); }

		const bool editorFrozen = simulationFrozen;

		auto clampObject = [](GameObject& object, bool limit) {

			// Recover any object that falls through the floor;
			if (object.rigidBody3D.translation.y < -1000.0f) {

				Entity* asEntity = dynamic_cast<Entity*>(&object);
				const Vector3 recovery = asEntity ? asEntity->getSpawnPoint() : Vector3{ 0, 5, 0 };

				object.rigidBody3D.Teleport(recovery);

				// Reset Velocity on Teleport
				object.rigidBody3D.SetVelocity(Vector3Zero());
			}

			if (object.rigidBody3D.translation.y < 0 && limit) {
				object.rigidBody3D.translation.y = 0;
				std::cout << "Reset: " << object.name << "'s Position \n";
			}
			};

		// Age out stale noise before anything emits this frame, so an event emitted
		// now is heard by entities later in the same frame rather than next one.
		
		// Frozen editor time must not expire events either, hence the delta passed
		// through unchanged only when the sim is actually running.
		if (!editorFrozen)
		{
			scene.soundField.Update(delta);

			// Ahead of the player and entity updates for the same reason: the hum a
			// running station makes this frame should reach the stalker this frame,
			// not next. The minigame's own update runs much later in this function
			// and is about the overlay, not about the noise the machine makes.
			scene.EmitStationNoise(delta);
		}

		/** Update Player **/
		// Every tick, whatever is on screen: the simulation never reads an overlay.
		// Under an overlay or the World Editor the client sends empty commands, so
		// the player stands still until it closes.
		scene.ForEachPlayer([&](int slot, Player& player) {
			if (editorFrozen)
			{
				player.rigidBody3D.SyncBroadPhaseBox();
				return;
			}
			const Sim::PlayerCommand command = slot < static_cast<int>(commands.size()) ? commands[slot] : Sim::PlayerCommand{};
			player.update3D(scene, command, delta);
			clampObject(player, scene.limitYBounds);
		});

		/* Update GameObjects */
		scene.gameMap.ForEachGameObject([&](GameObject& object) {
			if (editorFrozen) { object.rigidBody3D.SyncBroadPhaseBox(); return; }
			object.isSelectable = true;
			object.update(&scene, delta);
			clampObject(object, scene.limitYBounds);
			});

		/* Update Interactables */
		scene.gameMap.ForEachInteractable([&](InteractableObject& interactable) {
			if (editorFrozen) { interactable.rigidBody3D.SyncBroadPhaseBox(); return; }
			interactable.update(&scene, delta);
			clampObject(interactable, scene.limitYBounds);
			});

		/** Update Entities **/
		scene.gameMap.ForEachEntity([&](Entity& entity)
			{
				// Frozen entities are not culled either: an entity sitting at zero
				// health would otherwise be deleted out from under the inspector
				// examining it.
				if (editorFrozen) { entity.rigidBody3D.SyncBroadPhaseBox(); return; }

				bool shouldKill = entity.health <= 0;

				if (shouldKill)
				{
					// Only items are auto-removed here; other entity types keep their
					// (still-zero-health) entry until their own death handling runs.
					// Destroy only marks it, so this is safe mid-traversal.
					if (entity.type == OBJECT_ITEM) { entity.Destroy(); }
					return;
				}

				entity.update(&scene, delta);
				clampObject(entity, scene.limitYBounds);
			});

		// After the entities have settled, so the Director advises on the state the
		// stalker actually ended the frame in rather than the previous one.
		if (!editorFrozen) { scene.director.Update(&scene, delta); }


		// Sweep objects of every kind flagged by Destroy() — removal must happen
		// outside the update loops above, since erasing mid-iteration invalidates them
		scene.gameMap.SweepDestroyed(&scene);

		/* Update Collisions */
		// The solver applies positional corrections, so running it against a frozen
		// scene would push a just-placed object out of the wall it was deliberately
		// snapped flush against.
		if (!editorFrozen) { scene.SolveCollisions(8); }

		/* Catches */
		// After the collisions, so contact means what the solver left touching, and
		// outside every FSM, so a catch happens the same way in all four states.
		// The round is decided once, at the very end of the tick (rule C-1).
		if (!editorFrozen) { scene.CheckCatches(); scene.CheckEscapes(); }
		scene.ResolveRound();
	}
}
#pragma endregion