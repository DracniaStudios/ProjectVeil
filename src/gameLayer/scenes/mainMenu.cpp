#include "MainMenu.h"

#include <Objects/Interactable/LockedBox.h>
/*
static GameObject* selectObject(const Camera& cam)
{
	auto scene = SceneManager::getInstance().currentScene;
	auto player = scene->player;

	Ray selectRay = {
		cam.position,
		scene->camera->forward
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
*/
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
	scene->name = "Main Menu";
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;
	scene->gameMap.create(Vector3(100, 1, 100));
	
	// Add Player To Objects

	scene->player->name = "Player";
	scene->player->type = OBJECT_PLAYER;
	scene->player->id = PLAYER_ID;
	scene->player->rigidBody3D.Teleport(Vector3(0, 5, 0));
	scene->player->onEnable();
	scene->gameMap.saveEntity(*scene->player);
	
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
	
	for (int i = 0; i < 7; i++)
	{
		auto rng = std::ranlux24_base(std::random_device{}());
		auto box = InteractableObject(INTERACT_MINIGAME, i);
		std::string name = "Locked Box: ";
		name += static_cast<char>(i);
		box.name = name.c_str();
		box.type = OBJECT_GENERIC;
		box.defaultColor = getRandomColor(rng);
		box.rigidBody3D.Teleport(Vector3(0, 3, 10));
		box.rigidBody3D.scale = Vector3One();
		scene->gameMap.saveInteractable(box);
	}

	return scene;
}