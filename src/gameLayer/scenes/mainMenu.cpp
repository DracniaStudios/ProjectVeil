#include "mainMenu.h"

#include <SaveSystem.h>
#include <UIEngine.h>
#include <gameMain.h>

void Scene_MainMenuUpdate(float deltaTime)
{
	auto manager = &SceneManagement::SceneManager::getInstance();
	auto scene = manager->currentScene;
	auto player = scene->player;

}

void Scene_MainMenuDraw2D()
{
	auto manager = &SceneManagement::SceneManager::getInstance();
	auto scene = manager->currentScene;

	// Draw Main Menu
	{
		drawTextScaled("Main Menu", Rectangle{ 0.4f, 0.1f, 0.1f, 0.1f});;

		if (addButton("Play", Rectangle{ 0, 0, 100, 50 }, Rectangle{ 0.4f, 0.3f, 0.2f, 0.1f }, UIEngine::getInstance()))
		{
			std::cout << "Play Button Clicked\n";
			SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), 1);
		}
		if (addButton("Settings", Rectangle{ 0, 0, 100, 50 }, Rectangle{ 0.4f, 0.3f, 0.2f, 0.1f }, UIEngine::getInstance()))
		{
			std::cout << "Settings Button Clicked\n";
			// Enable Settings Menu
		}
		if (addButton("Exit", Rectangle{ 0, 0, 100, 50 }, Rectangle{ 0.4f, 0.3f, 0.2f, 0.1f }, UIEngine::getInstance()))
		{
			std::cout << "Exit Button Clicked\n";
			close_game();
		}
	}

};

void Scene_MainMenuDraw3D()
{
	auto manager = &SceneManagement::SceneManager::getInstance();
	auto scene = manager->currentScene;

	// Weird Interaction Between Rendering Ray and layer Objects

}

SceneManagement::Scene* Scene_MainMenuConstruct()
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();

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