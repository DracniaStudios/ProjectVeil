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

/** Unique Id Instancing **/
std::uint64_t InstanceID::getIdAndIncrement()
{
	std::uint64_t id = idCounter;
	idCounter++;
	permaAssertComment(id < UINT64_MAX - 1, "We ran out of ids somehow...");
	return id;
}

// Renumbers every object sequentially from 0.
void Scene::ResetID() {
	instanceHolder.idCounter = 0;

	// `id` is a monotonically increasing counter value, not an array position,
	// so it has no relationship to gameObjects.size() — comparing the two used
	// to delete any object whose id happened to be >= the current object
	// count. NewWorld() is this function's only caller, and it runs after
	// gameMap.create() has already assigned the fresh floor object whatever
	// idCounter was left at by the previous world, so `id >= size()` (1) was
	// true for that floor in any session where idCounter wasn't already 0 —
	// silently deleting it and leaving "New World" with an empty scene.
	for (auto& object : gameMap.gameObjects) {
		object.id = instanceHolder.getIdAndIncrement();
	}
}

/** Physics Solutions **/

// Refresh a body's collision box after a positional correction so subsequent
// solver iterations use the updated position rather than the stale one.
// Forwards to the rigid body so the solver, RigidBody3D::Update() and the
// editor's frozen path can never disagree on how a box is built.
static void refreshCollisionBox(RigidBody3D& body)
{
	body.SyncCollisionBox();
}

// Collision runs in two phases. The CheckCollisionBoxes tests below are the
// BROAD phase: rigidBody3D.collisionBox is the axis-aligned box enclosing each
// body, so if two of those miss, the oriented boxes inside them cannot touch.
// Only pairs that survive reach resolveConstrains, which runs the exact
// separating-axis test. Keeping the cheap gate matters — the narrow phase walks
// 15 axes per pair and this loop is O(n^2) per iteration.
static void solveCollision(Scene* scene, float delta, int solverIterations = 6)
{
	solverIterations = static_cast<int>(Clamp(static_cast<float>(solverIterations), 4, 8));

	auto& objects = scene->gameMap.gameObjects;

	for (int iter = 0; iter < solverIterations; iter++)
	{
		// Game Objects Vs. Game Objects — each unordered pair once (j starts
		// at i+1); resolveConstrains already moves both bodies, so visiting
		// (A,B) and (B,A) doubled every correction and collision event
		for (size_t i = 0; i < objects.size(); i++)
		{
			for (size_t j = i + 1; j < objects.size(); j++)
			{
				auto& bodyA = objects[i];
				auto& bodyB = objects[j];

				if (bodyA.rigidBody3D.canCollide == false || bodyB.rigidBody3D.canCollide == false) { continue; }
				if (bodyA.rigidBody3D.isStatic && bodyB.rigidBody3D.isStatic) { continue; }

				if (CheckCollisionBoxes(bodyA.rigidBody3D.collisionBox, bodyB.rigidBody3D.collisionBox))
				{
					bodyA.rigidBody3D.resolveConstrains(&bodyA, &bodyB);
					refreshCollisionBox(bodyA.rigidBody3D);
					refreshCollisionBox(bodyB.rigidBody3D);
				}
			}
		}

		// Entities Vs. Game Objects
		for (auto& [id, entity] : scene->entities)
		{
			for (auto& bodyB : objects)
			{
				if (CheckCollisionBoxes(entity->rigidBody3D.collisionBox, bodyB.rigidBody3D.collisionBox))
				{
					entity->rigidBody3D.resolveConstrains(entity.get(), &bodyB);
					refreshCollisionBox(entity->rigidBody3D);
					refreshCollisionBox(bodyB.rigidBody3D);
				}
			}
		}

		// Entities Vs. Entities — each unordered pair once
		for (auto bodyA = scene->entities.begin(); bodyA != scene->entities.end(); ++bodyA)
		{
			for (auto bodyB = std::next(bodyA); bodyB != scene->entities.end(); ++bodyB)
			{
				if (CheckCollisionBoxes(bodyA->second->rigidBody3D.collisionBox, bodyB->second->rigidBody3D.collisionBox))
				{
					bodyA->second->rigidBody3D.resolveConstrains(bodyA->second.get(), bodyB->second.get());
					refreshCollisionBox(bodyA->second->rigidBody3D);
					refreshCollisionBox(bodyB->second->rigidBody3D);
				}
			}
		}
		
		// Interactable Vs. Game Objects
		for (auto& [id, entity] : scene->interactables)
		{
			for (auto& bodyB : objects)
			{
				if (CheckCollisionBoxes(entity->rigidBody3D.collisionBox, bodyB.rigidBody3D.collisionBox))
				{
					entity->rigidBody3D.resolveConstrains(entity.get(), &bodyB);
					refreshCollisionBox(entity->rigidBody3D);
					refreshCollisionBox(bodyB.rigidBody3D);
				}
			}
		}

		// Interactable Vs. Entities — each unordered pair once
		for (auto bodyA = scene->interactables.begin(); bodyA != scene->interactables.end(); ++bodyA)
		{
			for (auto bodyB = scene->entities.begin(); bodyB != scene->entities.end(); ++bodyB)
			{
				if (CheckCollisionBoxes(bodyA->second->rigidBody3D.collisionBox, bodyB->second->rigidBody3D.collisionBox))
				{
					bodyA->second->rigidBody3D.resolveConstrains(bodyA->second.get(), bodyB->second.get());
					refreshCollisionBox(bodyA->second->rigidBody3D);
					refreshCollisionBox(bodyB->second->rigidBody3D);
				}
			}
		}
		
		// Interactable Vs. Interactable — each unordered pair once
		for (auto bodyA = scene->interactables.begin(); bodyA != scene->interactables.end(); ++bodyA)
		{
			for (auto bodyB = std::next(bodyA); bodyB != scene->interactables.end(); ++bodyB)
			{
				if (CheckCollisionBoxes(bodyA->second->rigidBody3D.collisionBox, bodyB->second->rigidBody3D.collisionBox))
				{
					bodyA->second->rigidBody3D.resolveConstrains(bodyA->second.get(), bodyB->second.get());
					refreshCollisionBox(bodyA->second->rigidBody3D);
					refreshCollisionBox(bodyB->second->rigidBody3D);
				}
			}
		}

		// Player Vs. Entities — the player lives outside both containers, so
		// without this pass it walks straight through every entity (it already
		// resolves against gameObjects in Player::update3D)
		/*
		if (auto player = scene->player)
		{
			for (auto& [id, entity] : scene->entities)
			{
				if (CheckCollisionBoxes(player->rigidBody3D.collisionBox, entity->rigidBody3D.collisionBox))
				{
					player->rigidBody3D.resolveConstrains(player, entity.get());
					refreshCollisionBox(player->rigidBody3D);
					refreshCollisionBox(entity->rigidBody3D);
				}
			}
		}

		if (auto player = scene->player)
		{
			for (auto& [id, entity] : scene->interactables)
			{
				if (CheckCollisionBoxes(player->rigidBody3D.collisionBox, entity->rigidBody3D.collisionBox))
				{
					player->rigidBody3D.resolveConstrains(player, entity.get());
					refreshCollisionBox(player->rigidBody3D);
					refreshCollisionBox(entity->rigidBody3D);
				}
			}
		}
		*/
	}
}

/** Scene Functions **/
void Scene_updateScene(float delta) {

	auto manager = &SceneManager::getInstance();
	auto worldEditor = &WorldEditor::getInstance();
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

	// While the editor holds the simulation, nothing may integrate. Physics used
	// to keep running underneath the editor, which meant a freshly placed object
	// fell under gravity the instant it existed, a dragged object fought the
	// solver for its own position, and lifeSpan/deathSpan ticked objects out
	// from under the person editing them.
	//
	// Collision boxes are still refreshed each frame: mouse picking reads them,
	// and RigidBody3D::Update() is the only thing that normally rebuilds them.
	const bool editorFrozen = worldEditor->IsEnabled() && worldEditor->IsSimulationPaused();

	auto clampObject = [](GameObject& object, bool limit) {
		// Clamp Y Bounds
		if (object.rigidBody3D.translation.y < -1000.0f) {
			object.rigidBody3D.Teleport(Vector3{ 0, 5, 0 });
		}

		if (object.rigidBody3D.translation.y < 0 && limit) {
			object.rigidBody3D.translation.y = 0;
			std::cout << "Reset: " << object.name << "'s Position \n";
		}
	};

	/** Update Player **/
	if (auto player = scene->player) {
		if (editorFrozen)
		{
			player->rigidBody3D.SyncCollisionBox();
		}
		else if (scene->is2DActive)
		{
			player->update2D(delta);
		}
		else
		{
			if (!worldEditor->IsEnabled()) { player->update3D(delta); };
			clampObject(*player, scene->limitYBounds);
		}
	}

	/* Update GameObjects */
	for (auto& object : scene->gameMap.gameObjects) {
		if (editorFrozen) { object.rigidBody3D.SyncCollisionBox(); continue; }
		object.isSelectable = true;
		object.update(scene, delta);
		clampObject(object, scene->limitYBounds);
	}

	/* Update Interactables */
	for (auto interactable = scene->interactables.begin(); interactable != scene->interactables.end();) {
		interactable->second->id = interactable->first;
		if (editorFrozen) { interactable->second->rigidBody3D.SyncCollisionBox(); ++interactable; continue; }
		interactable->second->update(scene, delta);
		clampObject(*interactable->second.get(), scene->limitYBounds);
		++interactable;
	}

	/** Update Entities **/
	for (auto entity = scene->entities.begin(); entity != scene->entities.end();)
	{
		// Update Data
		entity->second->id = entity->first;

		// Frozen entities are not culled either: an entity sitting at zero
		// health would otherwise be deleted out from under the inspector
		// examining it.
		if (editorFrozen) { entity->second->rigidBody3D.SyncCollisionBox(); ++entity; continue; }

		bool shouldKill = entity->second->health <= 0;

		if (shouldKill)
		{
			// Only items are auto-removed here; other entity types keep their
			// (still-zero-health) entry until their own death handling runs.
			if (entity->second->type != OBJECT_ITEM) { ++entity; continue; }

			// removeEntity() erases this key from scene->entities, invalidating
			// `entity` - grab the next iterator first.
			auto next = std::next(entity);
			scene->gameMap.removeEntity(entity->second.get());
			entity = next;
		}
		else
		{
			entity->second->update(scene, delta);

			clampObject(*entity->second.get(), scene->limitYBounds);
			++entity;
		}
	}


	// Sweep objects flagged by Destroy() — removal must happen outside the
	// update loop above, since erasing mid-iteration invalidates it
	std::erase_if(scene->gameMap.gameObjects, [&](GameObject& object) {
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
	if (auto miniGame = scene->miniGame)
	{
		scene->isMiniActive = true;
		miniGame->update(miniGame->data, scene->player, delta);
		
		if (miniGame->data->isComplete)
		{
			scene->is2DActive = false;
			scene->isMiniActive = false;
			scene->miniGame = nullptr;
			delete miniGame->data;
			delete miniGame;
		}
		else if (miniGame->data->isReset)
		{
			delete miniGame->data;
			delete miniGame;
			scene->SetMiniGame(scene->GetLastMiniGame());
		}
	}
	else
	{
		scene->isMiniActive = false;
	}

	// Pause To Inventory
	// Gated on isMiniActive so TAB can't be used to regain 3D player control
	// while a minigame is still running in the background.
	if (IsKeyPressed(KEY_TAB) && !scene->isMiniActive) { scene->is2DActive = !scene->is2DActive; }

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
				scene->miniGame->draw(scene->miniGame->data, scene->player);
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

		for (auto& object : scene->gameMap.gameObjects) {
			object.render3D();
		}

		for (auto& entity : scene->entities) {
			entity.second->render3D();
		}

		for (auto& interact : scene->interactables) {
			interact.second->render3D();
		}

		scene->player->render3D();
	}
}


void Scene::SetMiniGame(int value)
{
	if (value < MINI_GAME_FLAPPY_BIRD_ID || value > MINI_GAME_RO_SHAM_BOO_ID)
	{
		std::cout << "[Scene.cpp] Ignoring SetMiniGame with unknown id: " << value << "\n";
		return;
	}

	// Replacing an already-running minigame without freeing it would leak both
	// the MiniGame and its MiniGameData.
	if (miniGame) {
		delete miniGame->data;
		delete miniGame;
		miniGame = nullptr;
	}

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
}
