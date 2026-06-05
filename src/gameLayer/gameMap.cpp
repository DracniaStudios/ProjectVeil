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
	floor.isDestructable = false;

	floor.meshVariant = MESH_CUBE;
	floor.meshData = Vector4One();

	floor.rigidBody3D.isStatic = true;
	floor.rigidBody3D.translation = Vector3(0, -1, 0);
	floor.rigidBody3D.scale = size;
	floor.defaultColor = BLACK;

	saveObjectAt(floor.getPosition(), floor);

}

// Load Object into Scene (Vector3)
GameObject& GameMap::saveObjectAt(Vector3 position, GameObject& object)
{
	auto scene = SceneManager::getInstance().currentScene;
	auto id = scene->instanceHolder.getIdAndIncrement();

	object.id = id;

	/// Set RigidBody3D Data
	object.rigidBody3D.translation = position;
	if (object.rigidBody3D.scale == Vector3Zero())
	{
		object.rigidBody3D.scale = Vector3One();
	}

	/// Set GameMap Data
	object.onEnable();
	gameObjects.push_back(object);

	std::cout << "Added Object \n";
	return gameObjects.back();
}


Entity& GameMap::saveEntityAt(Vector3 position, Entity& entity)
{
	auto scene = SceneManager::getInstance().currentScene;
	
	// Sets Object ID and Adds To Scene
	saveObjectAt(entity.getPosition(), entity);

	/// Add Entity To Scene Entity Data
	auto entity_ptr = std::make_unique<Entity>(entity);
	scene->entities[entity.id] = std::move(entity_ptr);

	std::cout << "Added Object \n";
	return *scene->entities[entity.id].get();
}

// UnLoad Object From Scene
void GameMap::removeObject(GameObject* object) {
    if (!object) return;

	std::erase_if(SceneManager::getInstance().currentScene->gameMap.gameObjects, [&](GameObject& o){
        return &o == object;
    });
}