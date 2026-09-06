#include "Scene.h"

#include <SceneManager.h>
#include <WorldEditor.h>
#include <AudioManager.h>

#include <iterator>

Scene* Scene_new() {
	Scene* scene = new Scene;
	scene->gameMap = {};

	// Default Settings For Scene
	scene->player = new Player;
	scene->player->type = OBJECT_PLAYER;
	scene->player->id = PLAYER_ID;
	scene->player->onEnable();

	SceneManager::getInstance().currentScene = scene;
	return scene;
}

/** Task Stations **/

// Convenience wrapper over the GameMap-level scan, which lives there so
// MiniGame.h can reach it without a Scene. See FindRunningStation in gameMap.h.
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
//
// It emits at the *station*, not at the player. The minigame is a 2D overlay
// with no world position of its own, and using the player's would hand the
// stalker a live position feed wearing a station's clothes — the exact leak the
// sound-only decision exists to prevent. That the two happen to coincide while
// the player is working the station is the player's problem, not a fact
// perception was told.
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

// Renumbers every object sequentially from the first assignable id.
void Scene::ResetID() {
	// Start at 2 to avoid Object 0 and Player;
	gameMap.ResetGameObjectIds();
}

/** Physics Solutions **/

// Refresh a body's collision box after a positional correction so subsequent
// solver iterations use the updated position rather than the stale one.
// Forwards to the rigid body so the solver, RigidBody3D::Update() and the
// editor's frozen path can never disagree on how a box is built.
static void refreshBroadPhaseBox(RigidBody3D& body)
{
	body.SyncBroadPhaseBox();
}

// Collision runs in two phases. The OverlapsBroadPhase tests below are the
// BROAD phase: each body's broadPhaseBox is the axis-aligned box enclosing its
// collider, so if two of those miss, the colliders inside them cannot touch.
// Only pairs that survive reach resolveConstrains, which runs the exact
// separating-axis test. Keeping the cheap gate matters — the narrow phase walks
// 15 axes per pair and this loop is O(n^2) per iteration.
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
			if (bodyA.rigidBody3D.canCollide == false || bodyB.rigidBody3D.canCollide == false) { return true; }
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

/** Scene Functions **/
void Scene_updateScene(float delta) {

	auto manager = &SceneManager::getInstance();
	auto worldEditor = &WorldEditor::getInstance();
	auto inputSystem = &InputSystem::getInstance();
	auto scene = manager->currentScene;
	scene->update(delta);

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

	auto clampObject = [scene](GameObject& object, bool limit) {
		
		// Revcover any object that falls through the floor;
		if (object.rigidBody3D.translation.y < -1000.0f) {
			/*
			std::cout << "[Scene.cpp] Consider deleting fallen objects to prevent bugs \n";
			const Vector3 recovery = scene->gameMap.hasSpawnPoint
				? Vector3{ scene->gameMap.spawnPoint.x,
				           scene->gameMap.spawnPoint.y + 2.0f,
				           scene->gameMap.spawnPoint.z }
				: Vector3{ 0, 5, 0 };
			//object.rigidBody3D.Teleport(recovery);

			*/
			std::cout << "[Scene.cpp] Object " << object.name << " fell out of the world, recovering to spawn point \n";
			std::cout << "[Scene.cpp] !!! Modify these lines correctly to delete fallen objects and restore entities. !!! \n";

			// Reset Velocity on Teleport
			object.rigidBody3D.SetVelocity(Vector3Zero());

			std::cout << "[Scene] Recovered " << object.name
				<< " after falling out of the world\n";
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
	if (inputSystem->IsActionPressed(ACTION_UI_PAUSE)) {
		if (scene->isMiniActive) {
			scene->is2DActive = false;
			scene->ReleaseMiniGame();
		}
		else {
			scene->is2DActive = !scene->is2DActive;
		}
	}

	// World Editor (includes the developer tool windows)
	WorldEditor::getInstance().update(scene->player);
}

void Scene_drawScene2D() {
	auto manager = &SceneManager::getInstance();
	
	if (auto scene = manager->currentScene) {
		scene->draw2D();

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
		}

	}
}

void Scene_drawScene3D() {
	auto manager = &SceneManager::getInstance();
	if (auto scene = manager->currentScene) {
		if (WorldEditor::getInstance().IsEnabled()) { DrawGrid(100.0f, 1.0f); }
		
		scene->draw3D();

		scene->gameMap.ForEachObject([](GameObject& object) { object.render3D(); });

		scene->player->render3D();
	}
}

void Scene::ResetMiniGame() {
	// Release BEFORE asking for the replay.
			//
			// This path used to delete both allocations and leave the freed
			// pointer in scene->miniGame. That was survivable while SetMiniGame
			// simply overwrote the pointer, but SetMiniGame now frees whatever it
			// is replacing — so it saw the stale non-null pointer, dereferenced
			// it for ->data and freed the same two allocations a second time.
			// Every failed Flappy Bird or Crane attempt took that path.
	const int replayId = GetLastMiniGame();

	// Captured before the release, which clears it. A replay is the same player
	// still standing at the same station, so the station has to come back
	// occupied — otherwise it would fall silent for every attempt after the
	// first, and the Director would start hinting toward a machine the player is
	// currently using.
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
	// Before the delete, not after: this is the last moment the score the player
	// reached still exists anywhere. Every way out of a minigame — completing it,
	// failing it, pausing out of it — funnels through here, so capturing at this
	// one point covers all of them.
	SnapshotMiniGameData();

	if (miniGame != nullptr)
	{
		delete miniGame->data;
		delete miniGame;
	}

	// isRunningMiniGame was a one-way latch: ActivateMiniGame set it and nothing
	// ever cleared it, so a station counted as occupied for the rest of the
	// session. That was invisible until something depended on the flag; now two
	// things do. The station emitter would hum forever at a machine the player
	// walked away from, and the Director already skips occupied stations, so
	// every station the player ever touched was permanently excluded from
	// hinting. Releasing the minigame is the one point that knows the station is
	// free again. Clearing every flag rather than one remembered id also repairs
	// a save written while the latch was stuck.
	gameMap.ForEachInteractable([](InteractableObject& interactable) { interactable.isRunningMiniGame = false; });
	stationNoiseTimer = 0.0f;

	// Always null on return, whether or not there was anything to free. That is
	// the property every caller depends on — it is what makes a second release,
	// or a caller that bails out afterwards, harmless.
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
