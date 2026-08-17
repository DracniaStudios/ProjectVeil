#include "gameMap.h"

#include <SceneManager.h>
#include <algorithm>
#include <iostream>
#include <vector>
/** Map Data **/

// Create GameMap and Floor Space
void GameMap::create(Vector3 size)
{
	gameObjects.clear();
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

	// Named asset reference — bound in onEnable() and saved with the object
	floor.textureName = "Gravel041";

	saveObject(floor);

}

// Load Object into Scene (Vector3)
GameObject* GameMap::saveObject(GameObject& object)
{
	auto scene = SceneManager::getInstance().currentScene;
	object.id = scene->instanceHolder.getIdAndIncrement();

	/// Set RigidBody3D Data
	if (object.rigidBody3D.scale == Vector3Zero())
	{
		object.rigidBody3D.scale = Vector3One();
	}

	/// Set GameMap Data
	object.onEnable();
	gameObjects.push_back(object);

	std::cout << "Added Object \n";

	auto id = object.id;
	auto it = std::find_if(gameObjects.begin(), gameObjects.end(), [id](const GameObject& o) { return o.id == id; });
	return &(*it);
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
	scene->entities[entity.id] = std::move(entity.clone());

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
	return scene->interactables[object.id].get();
}

// Unload Object From Scene GameMap
void GameMap::removeObject(GameObject* object) {
    if (!object) return;
	auto scene = SceneManager::getInstance().currentScene;
	erase_if(scene->gameMap.gameObjects, [&](const GameObject& o) { return &o == object;});
}

// Remove Entity From Entity List
void GameMap::removeEntity(Entity* entity)
{
	// Entities live only in Scene::entities, not in gameObjects (see saveEntity),
	// so there is nothing to remove from gameObjects here.
	if (entity == nullptr) return;
	auto scene = SceneManager::getInstance().currentScene;
	scene->entities.erase(entity->id);
}

// Remove Interactable From InteractableList
void GameMap::removeInteractable(InteractableObject* object)
{
	// Interactables live only in Scene::interactables, not in gameObjects (see
	// saveInteractable), so there is nothing to remove from gameObjects here.
	if (object == nullptr) return;
	auto scene = SceneManager::getInstance().currentScene;
	// Player::inventory holds non-owning pointers into this map — drop any
	// reference before the unique_ptr below frees the object, or the next
	// inventory read (e.g. WorldEditor's Player Inspector) is a use-after-free.
	if (scene->player) { std::erase(scene->player->inventory, object); }
	scene->interactables.erase(object->id);
}

GameObject* GameMap::FindGameObjectByID(uint64_t id)
{
	const auto scene = SceneManager::getInstance().currentScene;
	for (size_t i = 0; i < scene->gameMap.gameObjects.size(); ++i) {
		auto obj = &scene->gameMap.gameObjects[i];
		if (obj->id == id) { return obj; }
	}
	return nullptr;
};

Entity* GameMap::FindEntityByID(uint64_t id)
{
	const auto scene = SceneManager::getInstance().currentScene;
	auto it = scene->entities.find(id);
	if (it == scene->entities.end()) { return nullptr; }
	return it->second.get();
};

InteractableObject* GameMap::FindInteractableByID(uint64_t id)
{
	const auto scene = SceneManager::getInstance().currentScene;
	auto it = scene->interactables.find(id);
	if (it == scene->interactables.end()) { return nullptr; }
	return it->second.get();
};

GameObject* GameMap::FindWorldObjectByID(uint64_t id)
{
	// 0 is the "no object" sentinel — InstanceID hands out ids from 2, reserving
	// 0 for "none" and 1 for the player.
	if (id == 0) { return nullptr; }

	// Interactables first: an id authored as an activator is an interactable in
	// every world shipped so far. The map lookups are O(1); the gameObjects scan
	// is linear, so it goes last.
	if (auto* interactable = FindInteractableByID(id)) { return interactable; }
	if (auto* entity = FindEntityByID(id)) { return entity; }
	return FindGameObjectByID(id);
};