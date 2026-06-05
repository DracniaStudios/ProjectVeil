#include "mainMenu.h"

bool isImGuiEnabled = false;

Player player{};
GameObject* selectedObject{};
Inventory inventory{};
int miniGameID = 0;

GameObject* selectObject(Camera& cam)
{

	auto scene = SceneManager::getInstance().currentScene;

	Ray selectRay;
	selectRay.position = cam.position; // Adjust the ray's origin to be at the player's head height
	selectRay.direction = player.camera.forward;

	if (scene->gameMap.gameObjects.size() > 0)
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

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float deltaTime)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);
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

	if (scene->isMiniActive && scene->miniGame != nullptr)
	{
		scene->miniGame->update(manager_ptr, &player, deltaTime);
	}

	/// Player Select Objects
	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
	{
		selectedObject = selectObject(cam);
	}

	/// Switch to 2D Mode
	if (IsKeyPressed(KEY_TAB))
	{
		scene->is2DActive = !scene->is2DActive;
	}
#pragma region ImGui
	DeveloperWindow::getInstance().update(&player);
	
#pragma endregion
}

void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);

	if (scene->is2DActive)
	{
		/// Background
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{20, 20, 20, 200});
		
		/// Inventory
		//inventory.render(assetManager);

		/// Mini Games On Top
		if (scene->isMiniActive && scene->miniGame != nullptr)
		{
			scene->miniGame->draw(manager_ptr, object_ptr);
		}
	}
	player.render2D();
	/// Always Render Player Last (On Top)
};

void Scene_MainMenuDraw3D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);

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
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;
	scene->object_ptr = scene;

	scene->gameMap.create(Vector3(100, 1, 100));
	
	// Add Player To Objects
	scene->gameMap.gameObjects.push_back(player);
	
	player.name = "Player";
	player.onEnable();
	player.rigidBody3D.teleport(Vector3(0, 5, 0));

	Entity target = {};
	target.name = "Target";
	target.type = OBJECT_ENTITY;
	target.defaultColor = RED;
	target.forceFire = false;
	target.rigidBody3D.translation = Vector3(5, 3, 0);
	target.rigidBody3D.scale = Vector3(2, 2, 2);
	target.rigidBody3D.isStatic = true;

	scene->gameMap.saveEntityAt(target.getPosition(), target);

	return scene;
}