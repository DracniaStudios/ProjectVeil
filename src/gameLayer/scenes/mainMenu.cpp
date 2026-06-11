#include "MainMenu.h"

#include <Objects/Interactable/LockedBox.h>

auto player = Player();

static GameObject* selectObject(const Camera& cam)
{

	auto scene = SceneManager::getInstance().currentScene;

	Ray selectRay = {
		cam.position,
		player.camera.forward
	};

	if (!scene->gameMap.gameObjects.empty())
	{
		for (auto& object : scene->gameMap.gameObjects)
		{
			if (object.isEnabled)
			{
				BoundingBox objectBox = {
					object.getPosition() - object.getSize() / 2,
					object.getPosition() + object.getSize() / 2
				};
				if (GetRayCollisionBox(selectRay, objectBox).hit)
				{
					if (!object.canBeSelected) { continue; }

					return &object;
				}
			}
		}
	}

	return nullptr;

}

void Scene_MainMenuUpdate(float deltaTime)
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	auto& cam = manager->camera3D;

	/// Update Scene Data
	if (scene->is2DActive)
	{
		player.update2D(deltaTime);
	}
	else
	{
		player.update3D(deltaTime);
		player.camera.UpdateCameraFPS(&cam, &player);
	}

	/** Possible Issue? **/
	if (scene->isMiniActive && scene->miniGame != nullptr)
	{
		scene->miniGame->update(&player, deltaTime);
	}

	/// Player Select Objects
	if (IsKeyPressed(KEY_ONE))
	{
		std::cout << "Activate Interaction \n";
		auto cameraRay = Ray(player.camera.position, player.camera.forward);
		for (auto& object : scene->gameMap.gameObjects)
		{
			auto collision = GetRayCollisionBox(cameraRay, object.rigidBody3D.collisionBox);
			if (collision.hit)
			{
				std::cout << "Hit Ray \n";
				if (InteractWithObject(&object))
				{
					std::cout << "Interacted \n";
				}
			}
		}
	}

	/// Switch to 2D Mode
	if (IsKeyPressed(KEY_TAB)){ scene->is2DActive = !scene->is2DActive;}

	/// Change Mini Game
	if (scene->miniGame != nullptr) { scene->isMiniActive = true; }
	if (IsKeyPressed(KEY_ONE)) { scene->miniGame = MiniGame_FlappyBird(&player); }
	if (IsKeyPressed(KEY_TWO)) { scene->miniGame = MiniGame_Crane(&player); }
	if (IsKeyPressed(KEY_THREE)) { scene->miniGame = MiniGame_Doctor(&player); }
	if (IsKeyPressed(KEY_FOUR)) { scene->miniGame = MiniGame_SimonSays(&player); }
	if (IsKeyPressed(KEY_FIVE)) { scene->miniGame = MiniGame_TimedSimonSays(&player); }
	if (IsKeyPressed(KEY_SIX)) { scene->miniGame = MiniGame_Maze(&player); }
	if (IsKeyPressed(KEY_SEVEN)) { scene->miniGame = MiniGame_RoShamBoo(&player); }

#pragma region ImGui
	DeveloperWindow::getInstance().update(&player);
	
#pragma endregion
}

void Scene_MainMenuDraw2D()
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;

	if (scene->is2DActive)
	{
		/// Background
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{20, 20, 20, 200});
		
		/// Inventory
		//inventory.render(assetManager);

		/// Mini Games On Top
		if (scene->isMiniActive && scene->miniGame != nullptr)
		{
			scene->miniGame->draw(&player);
		}
	}
	player.render2D();
	/// Always Render Player Last (On Top)
};

void Scene_MainMenuDraw3D()
{
	auto manager = &SceneManager::getInstance();
	auto scene = static_cast<Scene*>(manager->currentScene);

	DrawGrid(100.0f, 1.0f);
	
	for (auto& object : scene->gameMap.gameObjects) {
		object.render3D();
	}
	
	// Weird Interaction Between Rendering Ray and layer Objects

	player.render3D();
}

Scene* Scene_MainMenuConstruct()
{
	Scene* scene = Scene_new();
	scene->name = "Main Menu";
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;

	scene->gameMap.create(Vector3(100, 1, 100));
	
	// Add Player To Objects

	player.name = "Player";
	player.onEnable();
	player.rigidBody3D.Teleport(Vector3(0, 5, 0));
	scene->gameMap.saveEntity(player);
	
	Entity enemy = {};
	enemy.name = "Enemy";
	enemy.type = OBJECT_ENTITY;
	enemy.defaultColor = RED;
	enemy.rigidBody3D.Teleport(Vector3(5, 3, 0));
	enemy.rigidBody3D.scale = Vector3One();
	enemy.rigidBody3D.rotation = QuaternionFromVector3ToVector3(enemy.getPosition(), Vector3Subtract(enemy.getPosition(), enemy.rigidBody3D.down));
	enemy.rigidBody3D.isStatic = true;
	enemy.forceFire = false;
	scene->gameMap.saveEntity(enemy);

	GameObject target = {};
	target.name = "Target";
	target.type = OBJECT_GENERIC;
	target.defaultColor = RED;
	target.rigidBody3D.Teleport(Vector3(5, 3, 5));
	target.rigidBody3D.scale = Vector3One();
	target.rigidBody3D.isStatic = true;
	scene->gameMap.saveObject(target);
	
	LockedBox box = {};
	box.name = "Locked Box";
	box.type = OBJECT_GENERIC;
	box.defaultColor = Color(255, 191, 0, 255);
	box.rigidBody3D.Teleport(Vector3(0, 3, 10));
	box.rigidBody3D.scale = Vector3One();
	scene->gameMap.saveObject(box);

	return scene;
}