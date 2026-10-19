#include "Scene.h"

#include <SceneManager.h>
#include <WorldEditor.h>
#include <AudioManager.h>
#include <UIEngine.h>
#include <UI/Overlays.h>

#include <iterator>
#include <optional>
#include <random>

namespace SceneManagement {

	Scene* Scene_new() {
		Scene* scene = new Scene;
		scene->gameMap = {};

		// Default Settings For Scene
		scene->player = new Player;
		scene->player->type = OBJECT_PLAYER;
		scene->player->id = PLAYER_ID;

		// Player Spawn
		scene->player->setSpawnPoint(Vector3{ 0, 5, 2 });
		scene->player->rigidBody3D.Teleport(scene->player->getSpawnPoint());

		scene->player->onEnable();

		SceneManager::getInstance().currentScene = scene;
		return scene;
	}

	void Scene_delete(Scene* scene) {
		if (scene == nullptr) { return; }

		scene->QuitMiniGame();

		// ~GameObject() never frees GPU resources, so each generated fallback model
		// is released first, as GameMap::Destroy* does. Destroy* itself is not used:
		// DestroyInteractable edits currentScene's player, and by the time a scene is
		// freed currentScene is the scene that replaced it.
		scene->gameMap.ForEachGameObject([](GameObject& object) { object.releaseGeneratedModel(); });
		scene->gameMap.ForEachEntity([](Entity& entity) { entity.releaseGeneratedModel(); });
		scene->gameMap.ForEachInteractable([](InteractableObject& interactable) { interactable.releaseGeneratedModel(); });

		if (scene->player) {
			// The artifact is created on first use (InteractableObject.cpp) and owned by the player.
			if (scene->player->artifact) {
				scene->player->artifact->releaseGeneratedModel();
				delete scene->player->artifact;
			}
			scene->player->releaseGeneratedModel();
			delete scene->player;
		}

		delete scene;
	}



	// Renumbers every object sequentially from the first assignable id.
	void Scene::ResetID() {
		// Start at 2 to avoid Object 0 and Player;
		gameMap.ResetGameObjectIds();
	}

#pragma region Physics Management

	/** Physics Solutions **/
	static void refreshBroadPhaseBox(Physics3D::RigidBody3D& body)
	{
		body.SyncBroadPhaseBox();
	}

	// Collision runs in two phases.
	static void solveCollision(Scene* scene, float delta, int solverIterations = 6)
	{
		solverIterations = static_cast<int>(Clamp(static_cast<float>(solverIterations), 4, 8));

		auto& gameMap = scene->gameMap;

		for (int iter = 0; iter < solverIterations; iter++)
		{
			// Game Objects Vs. Game Objects — each unordered pair once;
			// resolveConstrains already moves both bodies, so visiting (A,B) and
			// (B,A) doubled every correction and collision event
			gameMap.ForEachObjectPair([&](GameObject& bodyA, GameObject& bodyB)
				{
					if (bodyA.rigidBody3D.collider.canCollide == false || bodyB.rigidBody3D.collider.canCollide == false) { return true; }
					if (bodyA.rigidBody3D.isStatic && bodyB.rigidBody3D.isStatic) { return true; }

					if (bodyA.rigidBody3D.OverlapsBroadPhase(bodyB.rigidBody3D))
					{
						bodyA.rigidBody3D.resolveConstrains(&bodyA, &bodyB);
						refreshBroadPhaseBox(bodyA.rigidBody3D);
						refreshBroadPhaseBox(bodyB.rigidBody3D);
					}
					return true;
				});

			// Entities Vs. Game Objects
			gameMap.ForEachEntity([&](Entity& entity)
				{
					gameMap.ForEachGameObject([&](GameObject& bodyB)
						{
							if (entity.rigidBody3D.OverlapsBroadPhase(bodyB.rigidBody3D))
							{
								entity.rigidBody3D.resolveConstrains(&entity, &bodyB);
								refreshBroadPhaseBox(entity.rigidBody3D);
								refreshBroadPhaseBox(bodyB.rigidBody3D);
							}
						});
				});

			// Entities Vs. Entities — each unordered pair once
			gameMap.ForEachEntityPair([&](Entity& bodyA, Entity& bodyB)
				{
					if (bodyA.rigidBody3D.OverlapsBroadPhase(bodyB.rigidBody3D))
					{
						bodyA.rigidBody3D.resolveConstrains(&bodyA, &bodyB);
						refreshBroadPhaseBox(bodyA.rigidBody3D);
						refreshBroadPhaseBox(bodyB.rigidBody3D);
					}
				});

			// Interactable Vs. Game Objects
			gameMap.ForEachInteractable([&](InteractableObject& entity)
				{
					gameMap.ForEachGameObject([&](GameObject& bodyB)
						{
							if (entity.rigidBody3D.OverlapsBroadPhase(bodyB.rigidBody3D))
							{
								entity.rigidBody3D.resolveConstrains(&entity, &bodyB);
								refreshBroadPhaseBox(entity.rigidBody3D);
								refreshBroadPhaseBox(bodyB.rigidBody3D);
							}
						});
				});

			// Interactable Vs. Entities — each unordered pair once
			gameMap.ForEachInteractable([&](InteractableObject& bodyA)
				{
					gameMap.ForEachEntity([&](Entity& bodyB)
						{
							if (bodyA.rigidBody3D.OverlapsBroadPhase(bodyB.rigidBody3D))
							{
								bodyA.rigidBody3D.resolveConstrains(&bodyA, &bodyB);
								refreshBroadPhaseBox(bodyA.rigidBody3D);
								refreshBroadPhaseBox(bodyB.rigidBody3D);
							}
						});
				});

			// Interactable Vs. Interactable — each unordered pair once
			gameMap.ForEachInteractablePair([&](InteractableObject& bodyA, InteractableObject& bodyB)
				{
					if (bodyA.rigidBody3D.OverlapsBroadPhase(bodyB.rigidBody3D))
					{
						bodyA.rigidBody3D.resolveConstrains(&bodyA, &bodyB);
						refreshBroadPhaseBox(bodyA.rigidBody3D);
						refreshBroadPhaseBox(bodyB.rigidBody3D);
					}
				});

			// Player Vs. Game Objects — the player lives outside both containers, so
			// without this pass it walks straight through every entity (it already
			// resolves against gameObjects in Player::update3D)

			if (auto player = scene->player) {
				gameMap.ForEachObject([&](GameObject& obj)
					{
						if (&obj == player) { return; }
						if (player->rigidBody3D.OverlapsBroadPhase(obj.rigidBody3D))
						{
							player->rigidBody3D.resolveConstrains(player, &obj);
							refreshBroadPhaseBox(player->rigidBody3D);
							refreshBroadPhaseBox(obj.rigidBody3D);
						}
					});
			}

			// Player Vs. Entities
			if (auto player = scene->player)
			{
				gameMap.ForEachEntity([&](Entity& entity)
					{
						if (player->rigidBody3D.OverlapsBroadPhase(entity.rigidBody3D))
						{
							player->rigidBody3D.resolveConstrains(player, &entity);
							refreshBroadPhaseBox(player->rigidBody3D);
							refreshBroadPhaseBox(entity.rigidBody3D);
						}
					});
			}

			// Player Vs. Interactables
			if (auto player = scene->player)
			{
				gameMap.ForEachInteractable([&](InteractableObject& entity)
					{
						if (player->rigidBody3D.OverlapsBroadPhase(entity.rigidBody3D))
						{
							player->rigidBody3D.resolveConstrains(player, &entity);
							refreshBroadPhaseBox(player->rigidBody3D);
							refreshBroadPhaseBox(entity.rigidBody3D);
						}
					});
			}
		}
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

		Vector2 WindowSize() { return Vector2{ static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) }; }

		// The player's input this frame, as the MiniGame reads it
		MiniGameInput ReadMiniGameInput()
		{
			auto& input = InputSystem::InputSystem::getInstance();
			const auto keys = [&input](auto query) {
				return MiniGameKeys{
					(input.*query)(InputSystem::ACTION_MOVE_LEFT), (input.*query)(InputSystem::ACTION_MOVE_RIGHT),
					(input.*query)(InputSystem::ACTION_MOVE_FORWARD), (input.*query)(InputSystem::ACTION_MOVE_BACKWARD),
					(input.*query)(InputSystem::ACTION_MOVE_JUMP), (input.*query)(InputSystem::ACTION_MOVE_SPRINT) };
			};

			MiniGameInput read;
			read.held = keys(&InputSystem::InputSystem::IsActionDown);
			read.pressed = keys(&InputSystem::InputSystem::IsActionPressed);
			read.screenSize = WindowSize();
			return read;
		}
	}

	bool Scene::StartMiniGame(int gameId, InteractableObject* station)
	{
		const std::uint64_t stationId = station != nullptr ? station->id : 0;
		if (!miniGameSession.Start(gameId, stationId, WindowSize(), std::random_device{}()))
		{
			std::cout << "[Scene.cpp] Ignoring a MiniGame start with unknown id: " << gameId << "\n";
			return false;
		}

		// A start replaces any game already running, so the station that one held is
		// free now; only the new one is occupied.
		gameMap.ForEachInteractable([](InteractableObject& interactable) { interactable.isRunningMiniGame = false; });
		stationNoiseTimer = 0.0f;
		if (station != nullptr) { station->isRunningMiniGame = true; }

		player->artifactMode = gameId;
		overlays.Open(Overlay::MiniGame);
		return true;
	}

	void Scene::TickMiniGame(float deltaTime)
	{
		// A game under another overlay waits: it can't be lost while it can't be seen
		if (overlays.Top() != Overlay::MiniGame || !miniGameSession.IsRunning()) { return; }

		const MiniGameResult result = miniGameSession.Tick(ReadMiniGameInput(), deltaTime);
		if (result.outcome == MiniGameOutcome::Won) { FinishWonMiniGame(result); }
	}

	void Scene::QuitMiniGame()
	{
		miniGameSession.Quit();
		CloseMiniGame();
	}

	void Scene::RetryMiniGame()
	{
		miniGameSession.Retry(WindowSize());
	}

	void Scene::WinMiniGame()
	{
		const MiniGameResult won = miniGameSession.Win();
		if (won.outcome == MiniGameOutcome::Won) { FinishWonMiniGame(won); }
	}

	void Scene::FinishWonMiniGame(const MiniGameResult& won)
	{
		if (const std::optional<MiniGameReward> reward = RewardFor(won.gameId))
		{
			if (reward->heal > 0.0f) { player->applyHealthValue(reward->heal, false); }
			if (CooldownTimer* buff = player->getBuff(reward->buff)) { buff->use(); }
		}

		// A Practice run has no station: it completes nothing and toggles nothing
		InteractableObject* station = won.stationId != 0 ? gameMap.FindInteractable(won.stationId) : nullptr;
		if (station != nullptr)
		{
			// Marked before the Target is looked up, because this is the point at which
			// the task is objectively finished. Letting a deleted door un-complete a
			// finished task would leave the Director hinting at it forever.
			station->isCompleted = true;
			station->isInteractable = false;

			if (GameObject* target = station->FindMiniGameTarget(gameMap))
			{
				if (target->isEnabled) { target->onDisable(); }
				else { target->onEnable(); }
			}
			else
			{
				std::cout << "[MiniGame] Target " << station->targetId << " no longer exists — nothing to open\n";
			}
		}

		CloseMiniGame();
	}

	void Scene::CloseMiniGame()
	{
		overlays.Close(Overlay::MiniGame);
		gameMap.ForEachInteractable([](InteractableObject& interactable) { interactable.isRunningMiniGame = false; });
		stationNoiseTimer = 0.0f;
	}
#pragma endregion

#pragma region Scene Update
	/** Scene Functions **/
	void Scene_updateScene(float delta) {

		auto manager = &SceneManager::getInstance();
		auto worldEditor = &Editor::WorldEditor::getInstance();
		auto inputSystem = &InputSystem::InputSystem::getInstance();
		auto scene = manager->currentScene;
		if (scene->update) { scene->update(delta); }

		// Swap Editor and Player Camera. The editor camera ignores overlays, so the
		// Main Menu world can still be flown around under its Title; the player's
		// camera only follows the mouse while gameplay has the player's input.
		if (worldEditor->IsEnabled()) {
			worldEditor->editorCamera.Update(&manager->camera3D);
		}
		else if (scene->overlays.GameplayHasInput()) {
			if (scene->player != nullptr) {
				scene->player->camera.UpdateCameraFPS(&manager->camera3D);
			}
			else {
				std::cerr << "No Camera Detected \n";
			}
		}

		const bool editorFrozen = worldEditor->IsEnabled() && worldEditor->IsSimulationPaused();

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
			scene->soundField.Update(delta);

			// Ahead of the player and entity updates for the same reason: the hum a
			// running station makes this frame should reach the stalker this frame,
			// not next. The minigame's own update runs much later in this function
			// and is about the overlay, not about the noise the machine makes.
			scene->EmitStationNoise(delta);
		}

		/** Update Player **/
		if (auto player = scene->player) {
			if (editorFrozen)
			{
				player->rigidBody3D.SyncBroadPhaseBox();
			}
			else if (scene->overlays.GameplayHasInput())
			{
				if (!worldEditor->IsEnabled()) { player->update3D(delta); };
				clampObject(*player, scene->limitYBounds);
			}
			// Under any overlay the player stands still until it closes; inside a
			// MiniGame the player is the game's own cursor
		}

		/* Update GameObjects */
		scene->gameMap.ForEachGameObject([&](GameObject& object) {
			if (editorFrozen) { object.rigidBody3D.SyncBroadPhaseBox(); return; }
			object.isSelectable = true;
			object.update(scene, delta);
			clampObject(object, scene->limitYBounds);
			});

		/* Update Interactables */
		scene->gameMap.ForEachInteractable([&](InteractableObject& interactable) {
			if (editorFrozen) { interactable.rigidBody3D.SyncBroadPhaseBox(); return; }
			interactable.update(scene, delta);
			clampObject(interactable, scene->limitYBounds);
			});

		/** Update Entities **/
		// DestroyEntity() erases from the same map ForEachEntity is iterating, so
		// dead items are collected here and destroyed in a second pass afterward
		// rather than mid-traversal.
		std::vector<std::uint64_t> deadItemEntityIds;
		scene->gameMap.ForEachEntity([&](Entity& entity)
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
					if (entity.type == OBJECT_ITEM) { deadItemEntityIds.push_back(entity.id); }
					return;
				}

				entity.update(scene, delta);
				clampObject(entity, scene->limitYBounds);
			});
		for (auto id : deadItemEntityIds) { scene->gameMap.DestroyEntity(id); }

		// After the entities have settled, so the Director advises on the state the
		// stalker actually ended the frame in rather than the previous one.
		if (!editorFrozen) { scene->director.Update(scene, delta); }


		// Sweep objects flagged by Destroy() — removal must happen outside the
		// update loop above, since erasing mid-iteration invalidates it
		scene->gameMap.EraseGameObjectsIf([&](GameObject& object) {
			if (!object.pendingDestroy) { return false; }
			object.onDestroy(scene);
			return true;
			});

		/* Update Collisions */
		// The solver applies positional corrections, so running it against a frozen
		// scene would push a just-placed object out of the wall it was deliberately
		// snapped flush against.
		if (!editorFrozen) { solveCollision(scene, delta, 8); }

		/** Update MiniGame **/
		scene->TickMiniGame(delta);

		// The Inventory action opens the Inventory during gameplay and otherwise
		// backs out of whatever overlay is on top. Backing out of a minigame quits
		// it, so it is ended here: the stack only records that it closed.
		if (inputSystem->IsActionPressed(InputSystem::ACTION_UI_INVENTORY)) {
			if (scene->overlays.InventoryPressed() == Overlay::MiniGame) {
				scene->QuitMiniGame();
			}
		}

		// World Editor (includes the developer tool windows)
		Editor::WorldEditor::getInstance().update(scene->player);
	}

	void Scene_drawScene2D() {
		auto manager = &SceneManager::getInstance();

		if (auto scene = manager->currentScene) {
			// Brackets every widget drawn this frame. The list is cleared here rather
			// than during update so the UI Inspector, which runs in update, sees the
			// whole of last frame's UI instead of an empty list.
			auto& ui = UI::UIEngine::getInstance();
			ui.beginFrame();

			if (scene->draw2D) { scene->draw2D(); }

			// The dimming and the top overlay's panel, Title through MiniGame
			UI::drawOverlays(*scene);

			// Over the overlays: the reticle during gameplay, the cursor in a MiniGame
			scene->player->render2D();

			ui.endFrame();
		}
	}

	void Scene_drawScene3D() {
		auto manager = &SceneManager::getInstance();
		if (auto scene = manager->currentScene) {
			if (Editor::WorldEditor::getInstance().IsEnabled()) { DrawGrid(100.0f, 1.0f); }

			if (scene->draw3D) { scene->draw3D(); }

			scene->gameMap.ForEachObject([](GameObject& object) { object.render3D(); });

			scene->player->render3D();
		}
	}
}
#pragma endregion