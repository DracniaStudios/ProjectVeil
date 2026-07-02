#include "MainMenu.h"

#include <format>

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
	
	GameObject target = {};
	target.name = "Target";
	target.type = OBJECT_GENERIC;
	target.defaultColor = RED;
	target.rigidBody3D.Teleport(Vector3(5, 3, 5));
	target.rigidBody3D.scale = Vector3One();
	target.rigidBody3D.isStatic = true;
	scene->gameMap.saveObject(target);
	
	for (auto i = 0; i < 10; i++)
	{
		Entity enemy = {};
		enemy.name = "Random Entity";
		enemy.type = OBJECT_ENTITY;
		enemy.defaultColor = RED;
		enemy.rigidBody3D.Teleport(Vector3(5, 3, 0));
		enemy.rigidBody3D.scale = Vector3One();
		enemy.rigidBody3D.rotation = QuaternionFromVector3ToVector3(enemy.getPosition(), Vector3Subtract(enemy.getPosition(), enemy.rigidBody3D.down));
		enemy.rigidBody3D.isStatic = true;
		enemy.model = LoadModelFromMesh(GenMeshCube(1, 1, 1));
		enemy.forceFire = false;
		scene->gameMap.saveEntity(enemy);
	}

	for (int i = 0; i < 7; i++)
	{
		auto rng = std::ranlux24_base(std::random_device{}());
		auto box = InteractableObject(INTERACT_MINIGAME, i);

		box.name = "Random Box";
		box.type = OBJECT_GENERIC;
		box.defaultColor = getRandomColor(rng);
		box.rigidBody3D.Teleport(Vector3(0, 3, 10));
		box.rigidBody3D.scale = Vector3One();
		box.model = LoadModelFromMesh(GenMeshCube(1, 1, 1));
		scene->gameMap.saveInteractable(box);
	}

	Entity skeleton = {};
	skeleton.name = "Skeleton";
	skeleton.type = OBJECT_ENTITY;
	skeleton.texture = AssetManager::getInstance().ice_01;
	skeleton.model = LoadModel(RESOURCES_PATH "models/Skeleton/FullHumanSkeleton.obj");
	skeleton.defaultColor = WHITE;
	skeleton.rigidBody3D.Teleport(Vector3(10, 30, 10));
	skeleton.rigidBody3D.scale = Vector3One();
	scene->gameMap.saveEntity(skeleton);


	return scene;
}