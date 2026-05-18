#include "mainMenu.h"

#include <DeveloperWindow.h>

bool isImGuiEnabled = false;

Player player = {};
AssetManager assetManager = {};
GameObject* selectedObject = {};
DeveloperWindow developerConsole = {};
int miniGameID = 0;

Vector3 cubePosition = { 0, 0, 0 };

GameObject* selectObject(Scene* scene, Camera& cam)
{
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

	scene->gameMap.gameObjects[0].rigidBody3D.scale = scene->gameMap.getMapSize();

	/// Update Scene Data
	if (scene->is2DActive)
	{
		player.update2D(manager, deltaTime);
	}
	else
	{
		player.update3D(manager, deltaTime);
		player.camera.UpdateCameraFPS(&cam, &player);
	}

	if (scene->isMiniActive)
	{
		if (scene->miniGame != nullptr)
		{
			scene->miniGame->update(manager_ptr, &player, deltaTime);
		}
	}

	/// Player Select Objects
	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
	{
		selectedObject = selectObject(scene, cam);
	}

	/// Switch to 2D Mode
	if (IsKeyPressed(KEY_TAB)) scene->is2DActive = !scene->is2DActive;

#pragma region ImGui
	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

	if (isImGuiEnabled) {
		developerConsole.render(manager);
		developerConsole.update(manager, &player);
	}
#pragma endregion
}

void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);

	if (scene->is2DActive)
	{
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{20, 20, 20, 200});
		if (scene->isMiniActive && scene->miniGame != nullptr)
		{
			scene->miniGame->draw(manager_ptr, object_ptr);
		}
		player.render2D();
	}
	/// Always Render Player Last (On Top)
};

void Scene_MainMenuDraw3D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);
	DrawGrid(100.0f, 1.0f);
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
	scene->gameMap.objectID++;
	player.onEnable();

	return scene;
}