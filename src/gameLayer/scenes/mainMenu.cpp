#include "mainMenu.h"

#include <Objects/Interactable/LockedBox.h>

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

};

void Scene_MainMenuDraw3D()
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;

	DrawGrid(100.0f, 1.0f);
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

	SaveSystem::LoadGame(RESOURCES_PATH "../saves/world.json", *scene);
	//SaveSystem::LoadGame(RESOURCES_PATH "../saves/mainMenu.json", *scene);
	return scene;
}