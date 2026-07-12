#include "gameMap.h"

#include <iostream>
#include <string>
#include <SceneManager.h>

/** Map Data **/

// Create GameMap and Floor Space
void GameMap::create(Vector3 size)
{
	this->size = size;

	auto rng = std::ranlux24_base(std::random_device{}());

	GameObject floor{};
	floor.name = "Floor";
	floor.canBeSelected = false;
	floor.isDestructible = false;
	floor.rigidBody3D.isStatic = true;
	floor.rigidBody3D.translation = Vector3(0, -1, 0);
	floor.rigidBody3D.scale = size;
	floor.defaultColor = getRandomColor(rng);

	floor.texture = GetAssetPtrByName("Gravel041");
	floor.model = LoadModelFromMesh(GenMeshCube(size.x, 1, size.z));

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
	entity.id = scene->instanceHolder.getIdAndIncrement();
	// Sets Object ID and Adds To Scene
	if (entity.type != OBJECT_PLAYER) {
		
		std::cout << "Added Object \n";
	}

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

	std::cout << "Added Entity \n";
	return scene->entities[entity.id].get();
}

InteractableObject* GameMap::saveInteractable(InteractableObject& object)
{
	auto scene = SceneManager::getInstance().currentScene;
	object.id = scene->instanceHolder.getIdAndIncrement();
	object.onEnable();

	auto interact_ptr = std::make_unique<InteractableObject>(object);
	scene->interactables[object.id] = std::move(interact_ptr);

	std::cout << "Add Interactable \n";
	gameObjects.push_back(object);
	return scene->interactables[object.id].get();
}

// Unload Object From Scene
void GameMap::removeObject(GameObject* object) {
    if (!object) return;
	auto scene = SceneManager::getInstance().currentScene;
	erase_if(scene->gameMap.gameObjects, [&](const GameObject& o) { return &o == object;});
}


void GameMap::removeEntity(Entity* entity)
{
	auto scene = SceneManager::getInstance().currentScene;
	scene->entities.erase(entity->id);
	erase_if(scene->gameMap.gameObjects, [&](const GameObject& o) { return &o.id == &entity->id;});
	//removeObject(entity);
}
void GameMap::removeInteractable(InteractableObject* object)
{
	SceneManager::getInstance().currentScene->interactables.erase(object->id);
	removeObject(object);
}

// Find GameObjects
void* FindGameObjectByID(int id)
{
	auto gameMap = &SceneManager::getInstance().currentScene->gameMap;

	for (auto& obj : gameMap->gameObjects)
	{
		if (obj.id == id) { return &obj; }
	}
	return nullptr;
}