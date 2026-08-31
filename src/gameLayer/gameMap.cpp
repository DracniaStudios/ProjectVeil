#include "gameMap.h"

#include <SceneManager.h>
#include <algorithm>
#include <iostream>
#include <vector>
/** Map Data **/

// Create GameMap and Floor Space
void GameMap::create(Vector3 size)
{
	// NewWorld() reuses this on a populated map. ~GameObject() is trivial and
	// never frees GPU resources (see removeObject()), so every object still on
	// its generated fallback cube would otherwise leak its Model the moment
	// this clear erases it.
	for (auto& object : gameObjects) { object.releaseGeneratedModel(); }
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

/** Unique Id Instancing **/
std::uint64_t InstanceID::getIdAndIncrement()
{
	std::uint64_t id = idCounter;
	idCounter++;
	permaAssertComment(id < UINT64_MAX - 1, "We ran out of ids somehow...");
	return id;
}

// Load Object into Scene (Vector3)
GameObject* GameMap::saveObject(GameObject& object)
{
	object.id = instanceHolder.getIdAndIncrement();

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
	entity.id = instanceHolder.getIdAndIncrement();
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
	// clone() rather than make_unique<Entity>(entity): the latter slices, so a
	// Stalker (or any other subclass) handed to this function was stored as a
	// bare Entity and lost its overrides before it ever ticked.
	//
	// No std::move around the call: clone() already returns a prvalue, and
	// wrapping it blocks the copy elision that would otherwise construct in
	// place.
	entities[entity.id] = entity.clone();

	std::cout << "Added Entity \n";
	return entities[entity.id].get();
}

InteractableObject* GameMap::saveInteractable(InteractableObject& object)
{
	object.id = instanceHolder.getIdAndIncrement();
	object.onEnable();

	auto interact_ptr = std::make_unique<InteractableObject>(object);
	interactables[object.id] = std::move(interact_ptr);

	std::cout << "Add Interactable \n";
	return interactables[object.id].get();
}

// Unload Object From Scene GameMap
void GameMap::removeObject(GameObject* object) {
    if (!object) return;

	// ~GameObject() is trivial and never frees GPU resources — release a
	// generated fallback model/mesh here or it leaks the moment this object
	// is erased.
	object->releaseGeneratedModel();
	
	erase_if(gameObjects, [&](const GameObject& o) { return &o == object;});
}

// Remove Entity From Entity List
void GameMap::removeEntity(Entity* entity)
{
	// Entities live only in Scene::entities, not in gameObjects (see saveEntity),
	// so there is nothing to remove from gameObjects here.
	if (entity == nullptr) return;
	// See removeObject() — the unique_ptr erase below destroys the Entity
	// without ever freeing its generated model.
	entity->releaseGeneratedModel();
	entities.erase(entity->id);
}

// Remove Interactable From InteractableList
void GameMap::removeInteractable(InteractableObject* object)
{
	if (object == nullptr) return;
	
	// Frees Memory Correctly
	object->releaseGeneratedModel();
	auto scene = SceneManager::getInstance().currentScene;
	
	// Player Inventory can hold references to InteractableObjects
	if (scene->player) {
		std::erase(scene->player->inventory, object);
		if (scene->player->interactObjectId == object->id) {
			scene->player->interactObjectId = 0;
		}
	}
	interactables.erase(object->id);
}

