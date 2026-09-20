#include "Tutorial.h"

void Scene_TutorialUpdate(float deltaTime)
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	auto player = scene->player;

}

void Scene_TutorialDraw2D()
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;

};

void Scene_TutorialDraw3D()
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;

	// Weird Interaction Between Rendering Ray and layer Objects

}

Scene* Scene_TutorialConstruct()
{
	Scene* scene = Scene_new();

	// Load Main Menu World
	scene->name = "Tutorial";
	scene->update = Scene_TutorialUpdate;
	scene->draw2D = Scene_TutorialDraw2D;
	scene->draw3D = Scene_TutorialDraw3D;

	scene->player->setSpawnPoint(Vector3(20, 2, 0));
	scene->player->rigidBody3D.Teleport(Vector3(20, 2, 0));

	SaveSystem::LoadWorld("chunk_1", *scene);

	return scene;
}