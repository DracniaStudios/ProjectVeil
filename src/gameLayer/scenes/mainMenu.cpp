#include "mainMenu.h"

#include <SaveSystem.h>
#include <UIEngine.h>

void Scene_MainMenuUpdate(float deltaTime)
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	auto player = scene->player;

}

void Scene_MainMenuDraw2D()
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;

	// Draw Main Menu
	{
		drawText("Main Menu", Rectangle{ 0.1f, 0.8f, 0.1f, 0.1f});;

		if (addButton("Play", UIEngine::getInstance()))
		{
			std::cout << "Play Button Clicked\n";
			SceneManager_push(&SceneManager::getInstance(), 1);
		}
	}

};

void Scene_MainMenuDraw3D()
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;

	// Weird Interaction Between Rendering Ray and layer Objects

}

Scene* Scene_MainMenuConstruct()
{
	Scene* scene = Scene_new();

	// Load Main Menu World
	scene->name = "Main Menu";
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;
   

	scene->player->setSpawnPoint(Vector3(0, 2, 0));
	scene->player->rigidBody3D.Teleport(Vector3(0, 2, 0));

	SaveSystem::LoadWorld("mainMenu", *scene);

	return scene;
}