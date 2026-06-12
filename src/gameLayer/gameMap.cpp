#include "gameMap.h"

#include <iostream>
#include <string>
#include <SceneManager.h>

/** Map Data **/

// Create GameMap and Floor Space
void GameMap::create(Vector3 size)
{
	this->size = size;

	GameObject floor{};
	floor.name = "Floor";
	floor.canBeSelected = false;
	floor.isDestructible = false;

	floor.meshVariant = MESH_CUBE;
	floor.meshData = Vector4One();

	floor.rigidBody3D.isStatic = true;
	floor.rigidBody3D.translation = Vector3(0, -1, 0);
	floor.rigidBody3D.scale = size;
	floor.defaultColor = BLACK;

	saveObject(floor);

}

// Load Object into Scene (Vector3)
GameObject* GameMap::saveObject(GameObject& object)
{
	auto scene = SceneManager::getInstance().currentScene;
	auto id = scene->instanceHolder.getIdAndIncrement();

	object.id = id;

	/// Set RigidBody3D Data
	if (object.rigidBody3D.scale == Vector3Zero())
	{
		object.rigidBody3D.scale = Vector3One();
	}

	/// Set GameMap Data
	object.onEnable();
	gameObjects.push_back(object);

	std::cout << "Added Object \n";
	return &gameObjects.back();
}


Entity* GameMap::saveEntity(Entity& entity)
{
	auto scene = SceneManager::getInstance().currentScene;
	auto id = scene->instanceHolder.getIdAndIncrement();
	// Sets Object ID and Adds To Scene
	if (entity.type != OBJECT_PLAYER) {
		
		std::cout << "Added Object \n";
	}
	entity.id = id;

	/// Set GameMap Data
	entity.onEnable();

	/// Set RigidBody3D Data
	if (entity.rigidBody3D.scale == Vector3Zero())
	{
		entity.rigidBody3D.scale = Vector3One();
	}
	/// Add Entity To Scene Entity Data
	auto entity_ptr = std::make_unique<Entity>(entity);
	scene->entities[entity.id] = std::move(entity_ptr);

	std::cout << "Added Object \n";
	gameObjects.push_back(entity);
	return scene->entities[entity.id].get();
}

// UnLoad Object From Scene
void GameMap::removeObject(GameObject* object) {
    if (!object) return;
	auto scene = SceneManager::getInstance().currentScene;
	erase_if(scene->gameMap.gameObjects, [&](const GameObject& o) { return &o == object;});
}
void GameMap::removeEntity(Entity* entity)
{
	entity->health = -9999;
	removeObject(entity);
}