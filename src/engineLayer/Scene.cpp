#include "Scene.h"

#include <SceneManager.h>
#include <WorldEditor.h>
#include <AudioManager.h>
#include <UIEngine.h>

#include <iterator>

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

		scene->ReleaseMiniGame();

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

	void Scene::SnapshotMiniGameData()
	{
		if (miniGame == nullptr || miniGame->data == nullptr) { return; }

		currentMiniGameData = *miniGame->data;
		hasCurrentMiniGameData = true;
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

	void Scene::ResetMiniGame() {
		// Release BEFORE asking for the replay.
		const int replayId = GetLastMiniGame();

		const InteractableObject* station = GetRunningStation();
		const std::uint64_t stationId = station != nullptr ? station->id : 0;

		if (miniGame != nullptr) {
			ReleaseMiniGame();
		}
		SetMiniGame(replayId);

		// Nothing to replay: hand the player back to the 3D world rather
		// than stranding them in an empty 2D overlay with no way out.
		if (miniGame == nullptr) { is2DActive = false; return; }

		if (stationId != 0)
		{
			if (auto* resumed = gameMap.FindInteractable(stationId))
			{
				resumed->isRunningMiniGame = true;
			}
		}
	}

	void Scene::ReleaseMiniGame()
	{
		SnapshotMiniGameData();

		if (miniGame != nullptr)
		{
			delete miniGame->data;
			delete miniGame;
		}

		gameMap.ForEachInteractable([](InteractableObject& interactable) { interactable.isRunningMiniGame = false; });
		stationNoiseTimer = 0.0f;

		miniGame = nullptr;
		isMiniActive = false;
		is2DActive = false;
	}

	void Scene::SetMiniGame(int value)
	{
		if (value < MINI_GAME_FLAPPY_BIRD_ID || value > MINI_GAME_RO_SHAM_BOO_ID)
		{
			std::cout << "[Scene.cpp] Ignoring SetMiniGame with unknown id: " << value << "\n";
			return;
		}

		// Roll the history back one slot before the incoming game takes the current
		// one. Snapshot first: the live data is about to be freed, and what it holds
		// right now — the score the player actually reached — is the whole point of
		// keeping it. Ordered ahead of ReleaseMiniGame for that reason.
		SnapshotMiniGameData();
		previousMiniGameData = currentMiniGameData;
		hasPreviousMiniGameData = hasCurrentMiniGameData;

		// Replacing an already-running minigame without freeing it would leak both
		// the MiniGame and its MiniGameData.
		ReleaseMiniGame();

		player->rigidBody2D = {};
		is2DActive = true;
		isMiniActive = true;

		player->artifactMode = value;

		switch (value)
		{
		case MINI_GAME_FLAPPY_BIRD_ID:
			miniGame = MiniGame_FlappyBird(player);
			break;
		case MINI_GAME_CRANE_ID:
			miniGame = MiniGame_Crane(player);
			break;
		case MINI_GAME_DOCTOR_ID:
			miniGame = MiniGame_Doctor(player);
			break;
		case MINI_GAME_SIMON_SAYS_ID:
			miniGame = MiniGame_SimonSays(player);
			break;
		case MINI_GAME_MAZE_ID:
			miniGame = MiniGame_Maze(player);
			break;
		case MINI_GAME_RO_SHAM_BOO_ID:
			miniGame = MiniGame_RoShamBoo(player);
			break;
		}

		if (miniGame) {
			lastMiniGamePlayed = value;
		}

		// The incoming game's opening state. A constructor that failed to produce a
		// game leaves the slot empty rather than carrying the outgoing game's score
		// forward under the new game's name.
		if (miniGame != nullptr && miniGame->data != nullptr)
		{
			currentMiniGameData = *miniGame->data;
			hasCurrentMiniGameData = true;
		}
		else
		{
			currentMiniGameData = {};
			hasCurrentMiniGameData = false;
		}
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

		// Swap Editor and Player Camera
		if (!scene->is2DActive) {
			if (scene->player != nullptr && worldEditor->IsEnabled() == false) {
				scene->player->camera.UpdateCameraFPS(&manager->camera3D);
			}
			else if (worldEditor->IsEnabled()) {
				worldEditor->editorCamera.Update(&manager->camera3D);
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
			else if (scene->is2DActive)
			{
				player->update2D(delta, player->rigidBody2D.canMove);
			}
			else
			{
				if (!worldEditor->IsEnabled()) { player->update3D(delta); };
				clampObject(*player, scene->limitYBounds);
			}
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
		if (auto miniGame = scene->miniGame; miniGame != nullptr)
		{
			scene->isMiniActive = true;
			miniGame->update(scene, delta);
		}
		else
		{
			scene->isMiniActive = false;
		}

		// Pause To Inventory
		// Gated on isMiniActive so TAB can't be used to regain 3D player control
		// while a minigame is still running in the background.
		if (inputSystem->IsActionPressed(InputSystem::ACTION_UI_PAUSE)) {
			if (scene->isMiniActive) {
				scene->is2DActive = false;
				scene->ReleaseMiniGame();
			}
			else {
				scene->is2DActive = !scene->is2DActive;
			}
		}

		PlayerUI::getInstance().update();
		UIEngine::getInstance().update();

		// World Editor (includes the developer tool windows)
		Editor::WorldEditor::getInstance().update(scene->player);
	}

	void Scene_drawScene2D() {
		auto manager = &SceneManager::getInstance();

		if (auto scene = manager->currentScene) {
			if (scene->draw2D) { scene->draw2D(); }

			if (scene->is2DActive)
			{
				/// Background
				DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 20, 20, 20, 200 });

				/// Inventory
				//inventory.render(assetManager);

				/// Mini Games On Top
				if (scene->isMiniActive && scene->miniGame != nullptr)
				{
					scene->miniGame->draw(scene);
				}
				scene->player->render2D();

				PlayerUI::getInstance().render();

			}

			UIEngine::getInstance().render();

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