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
	
	scene->player->artifact = new GameObject;
	scene->player->artifact->name = "Artifact";
	scene->player->artifact->isDestructible = false;
	scene->player->artifact->rigidBody3D.scale = Vector3(0.1f, 0.1f, 0.1f);
	scene->player->artifact->rigidBody3D.canCollide = false;
	scene->player->artifact->rigidBody3D.SetGravity(0, 0, 0);
	scene->player->artifact->setModel("RubixCube");
	scene->player->artifact->defaultSound = "Artifact_Load_Up";

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

/** Physics Solutions **/

// Refresh a body's collision box after a positional correction so subsequent
// solver iterations use the updated position rather than the stale one
static void refreshCollisionBox(RigidBody3D& body)
{
	body.collisionBox = {
		Vector3Subtract(body.translation, Vector3Scale(body.scale, 0.5f)),
		Vector3Add(body.translation,      Vector3Scale(body.scale, 0.5f))
	};
}

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

		// Player Vs. Entities — the player lives outside both containers, so
		// without this pass it walks straight through every entity (it already
		// resolves against gameObjects in Player::update3D)
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
			std::cout << "No Camera Detected \n";
		}
	}

	/** Update Player **/
	if (auto player = scene->player) {
		if (scene->is2DActive)
		{
			player->update2D(delta);
		}
		else
		{
			if (!worldEditor->IsEnabled()) { player->update3D(delta); };
		
			// Clamp Y Bounds
			if (player->rigidBody3D.translation.y < -1000.0f) {
				player->rigidBody3D.Teleport(Vector3{ 0, 5, 0 });
			}
			if (player->rigidBody3D.translation.y < 0 && scene->limitYBounds) {
				player->rigidBody3D.translation.y = 0;
			}

		}
	}

	/** Update GameObjects **/
	for (auto entity = scene->entities.begin(); entity != scene->entities.end();)
	{
		// Update Data
		entity->second->id = entity->first;
		
		bool shouldKill = false;

		if (entity->second->health <= 0)
		{
			shouldKill = true;
		}

		if (shouldKill)
		{
			// Check If Item
			if (entity->second->type != OBJECT_ITEM) { continue; }
			scene->gameMap.removeEntity(entity->second.get());
			//entity = scene->entities.erase(entity);
		}
		else
		{
			entity->second->update(scene, delta);
			// Clamp Y Bounds
			if (entity->second->rigidBody3D.translation.y < -1000.0f) {
				entity->second->rigidBody3D.Teleport(Vector3{ 0, 5, 0 });
			}
			if (entity->second->rigidBody3D.translation.y < 0 && scene->limitYBounds) {
				entity->second->rigidBody3D.translation.y = 0;
			}
			++entity;
		}
	}

	/* Limit GameObject Positions */
	for (auto& object : scene->gameMap.gameObjects) {
		object.update(scene, delta);
		
		// Clamp Y Bounds
		if (object.rigidBody3D.translation.y < -1000.0f) {
			object.rigidBody3D.Teleport(Vector3{ 0, 5, 0 });
		}
		if (object.rigidBody3D.translation.y < -1 && scene->limitYBounds) {
			object.rigidBody3D.Teleport(Vector3{ object.getPosition().x, 5, object.getPosition().z });
			std::cout << "Reset " << object.name << "'s Position \n";
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
	solveCollision(scene, delta, 8);

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
		}

		if (miniGame->data->isReset)
		{
			scene->SetMiniGame(scene->GetLastMiniGame());
		}


	}
	else
	{
		scene->isMiniActive = false;
	}

	// Pause To Inventory
	if (IsKeyPressed(KEY_TAB)) { scene->is2DActive = !scene->is2DActive; }

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
				scene->miniGame->draw(scene->miniGame->data, &scene->player);
			}
			scene->player->render2D();
		}

	}
}

void Scene_drawScene3D() {
	auto manager = &SceneManager::getInstance();
	if (auto scene = manager->currentScene) {
		scene->draw3D();

		for (auto& object : scene->gameMap.gameObjects) {
			object.render3D();
		}

		for (auto& entity : scene->entities) {
			entity.second->render3D();
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
