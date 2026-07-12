#include "Scene.h"
#include <SceneManager.h>
#include <WorldEditor.h>

Scene* Scene_new() {
	Scene* scene = new Scene;
	scene->player = new Player;
	scene->camera = new PlayerCamera;

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
static void solveCollision(Scene* scene, float delta, int solverIterations = 6)
{
	solverIterations = static_cast<int>(Clamp(solverIterations, 4, 8));
	for (int iter = 0; iter < solverIterations; iter++)
	{
		// Update Game Objects Vs. Game Objects
		for (auto& bodyA : scene->gameMap.gameObjects)
		{
			//permaAssertComment(&bodyA == nullptr, "Null bodyA @ Scene.cpp");
			for (auto& bodyB : scene->gameMap.gameObjects)
			{
				//permaAssertComment(&bodyB == nullptr, "Null bodyB @ Scene.cpp");

				if (CheckCollisionBoxes(bodyA.rigidBody3D.collisionBox, bodyB.rigidBody3D.collisionBox))
				{
					bodyA.rigidBody3D.resolveConstrains(&bodyA, &bodyB);
				}
			}

			// Refresh the collision box after each correction so subsequent
			// iterations use the updated position rather than the stale one
			bodyA.rigidBody3D.collisionBox = {
				Vector3Subtract(bodyA.rigidBody3D.translation, Vector3Scale(bodyA.rigidBody3D.scale, 0.5f)),
				Vector3Add(bodyA.rigidBody3D.translation,      Vector3Scale(bodyA.rigidBody3D.scale, 0.5f))
			};
		}

		// Update Entities Vs. Game Objects
		for (auto bodyA = scene->entities.begin(); bodyA != scene->entities.end(); ++bodyA)
		{
			//permaAssertComment(&bodyA == nullptr, "Null bodyA @ Scene.cpp");
			for (auto& bodyB : scene->gameMap.gameObjects)
			{
				//permaAssertComment(&bodyB == nullptr, "Null bodyB @ Scene.cpp");

				if (CheckCollisionBoxes(bodyA->second->rigidBody3D.collisionBox, bodyB.rigidBody3D.collisionBox))
				{
					bodyA->second->rigidBody3D.resolveConstrains(bodyA->second.get(), &bodyB);
				}
			}

			// Refresh the collision box after each correction so subsequent
			// iterations use the updated position rather than the stale one
			bodyA->second->rigidBody3D.collisionBox = {
				Vector3Subtract(bodyA->second->rigidBody3D.translation, Vector3Scale(bodyA->second->rigidBody3D.scale, 0.5f)),
				Vector3Add(bodyA->second->rigidBody3D.translation,      Vector3Scale(bodyA->second->rigidBody3D.scale, 0.5f))
			};
		}

		// Update Entities Vs. Entity
		for (auto bodyA = scene->entities.begin(); bodyA != scene->entities.end(); ++bodyA)
		{
			//permaAssertComment(&bodyA == nullptr, "Null bodyA @ Scene.cpp");
			for (auto bodyB = scene->entities.begin(); bodyB != scene->entities.end(); ++bodyB)
			{
				//permaAssertComment(&bodyB == nullptr, "Null bodyB @ Scene.cpp");

				if (CheckCollisionBoxes(bodyA->second->rigidBody3D.collisionBox, bodyB->second->rigidBody3D.collisionBox))
				{
					bodyA->second->rigidBody3D.resolveConstrains(bodyA->second.get(), bodyB->second.get());
				}
			}

			// Refresh the collision box after each correction so subsequent
			// iterations use the updated position rather than the stale one
			bodyA->second->rigidBody3D.collisionBox = {
				Vector3Subtract(bodyA->second->rigidBody3D.translation, Vector3Scale(bodyA->second->rigidBody3D.scale, 0.5f)),
				Vector3Add(bodyA->second->rigidBody3D.translation,      Vector3Scale(bodyA->second->rigidBody3D.scale, 0.5f))
			};
		}
	}
}

/** Scene Functions **/
void Scene_updateScene(float delta) {

	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	scene->update(delta);

	/** Update Player **/
	if (auto player = scene->player) {
		if (scene->is2DActive)
		{
			player->update2D(delta);
		}
		else
		{
			player->update3D(delta);
			scene->camera->UpdateCameraFPS(&manager->camera3D);
		
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
		if (scene->entities.empty()) return;

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
			//if (entity.second->type != OBJECT_ITEM) { continue; }
			entity = scene->entities.erase(entity);
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

	// Developer Window
	DeveloperWindow::getInstance().update(scene->player);

	// World Editor
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
	auto scene = manager->currentScene;
	if (scene != nullptr) { scene->draw3D(); }

	for (auto& object : scene->gameMap.gameObjects) {
		object.render3D();
	}

	for (auto& entity : scene->entities) {
		entity.second->render3D();
	}

	scene->player->render3D();
}


void Scene::SetMiniGame(int value)
{
	player->rigidBody2D = {};
	is2DActive = true;
	isMiniActive = true;
	switch (value)
	{
		case 0:
			miniGame = MiniGame_FlappyBird(player);
			break;
		case 1:
			miniGame = MiniGame_Crane(player);
			break;
		case 2:
			miniGame = MiniGame_Doctor(player);
			break;
		case 3:
			miniGame = MiniGame_SimonSays(player);
			break;
		case 4:
			miniGame = MiniGame_TimedSimonSays(player);
			break;
		case 5:
			miniGame = MiniGame_Maze(player);
			break;
		case 6:
			miniGame = MiniGame_RoShamBoo(player);
			break;
	}
	if (miniGame) {
		lastMiniGamePlayed = value;
	}
}
