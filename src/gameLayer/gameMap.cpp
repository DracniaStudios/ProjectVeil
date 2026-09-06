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
	// never frees GPU resources (see DestroyGameObject()), so every object still
	// on its generated fallback cube would otherwise leak its Model the moment
	// this clear erases it.
	for (auto& [id, object] : gameObjects) { object.releaseGeneratedModel(); }
	gameObjects.clear();
	this->size = size;
	auto rng = std::ranlux24_base(std::random_device{}());

	GameObject floor{};
	floor.name = "Floor";
	floor.isSelectable = false;
	floor.isDestructible = false;
	floor.rigidBody3D.isStatic = true;
	floor.rigidBody3D.translation = Vector3(0, -1, 0);
	floor.rigidBody3D.scale = size;
	floor.defaultColor = getRandomColor(rng);

	// Named asset reference — bound in onEnable() and saved with the object
	floor.textureName = "Gravel041";

	SpawnGameObject(floor);

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
GameObject* GameMap::SpawnGameObject(GameObject& object)
{
	object.id = instanceHolder.getIdAndIncrement();

	/// Set RigidBody3D Data
	if (object.rigidBody3D.scale == Vector3Zero())
	{
		object.rigidBody3D.scale = Vector3One();
	}

	/// Set GameMap Data
	object.onEnable();
	auto [it, inserted] = gameObjects.emplace(object.id, object);

	std::cout << "Added Object \n";

	return &it->second;
}


Entity* GameMap::SpawnEntity(Entity& entity)
{
	// Sets Object ID and Adds To Scene
	if (entity.type != OBJECT_PLAYER) {

		std::cout << "Added Object \n";
	}
	
	// Use clone() to preserve the dynamic type of the passed-in entity (e.g., Stalker)
	entities[entity.id] = entity.clone();
	
	// Sets Object ID and Adds To Scene
	auto ent = entities[entity.id].get();
	ent->id = instanceHolder.getIdAndIncrement();
	
	ent->onEnable();

	/// Set RigidBody3D Data
	if (ent->rigidBody3D.scale == Vector3Zero())
	{
		ent->rigidBody3D.scale = Vector3One();
	}

	ent->setSpawnPoint(ent->getPosition());
	ent->rigidBody3D.Teleport(ent->getSpawnPoint());

	std::cout << "Added Entity \n";
	return entities[entity.id].get();
}

InteractableObject* GameMap::SpawnInteractable(InteractableObject& object)
{
	object.id = instanceHolder.getIdAndIncrement();
	object.onEnable();

	auto interact_ptr = std::make_unique<InteractableObject>(object);
	interactables[object.id] = std::move(interact_ptr);

	std::cout << "Add Interactable \n";
	return interactables[object.id].get();
}

GameObject* GameMap::LoadGameObject(GameObject object)
{
	auto id = object.id;
	auto [it, inserted] = gameObjects.insert_or_assign(id, std::move(object));
	it->second.onEnable();
	return &it->second;
}

void GameMap::LoadEntity(std::unique_ptr<Entity> entity)
{
	auto id = entity->id;
	entities[id] = std::move(entity);
	entities[id]->onEnable();
	if (entities[id]->getSpawnPoint().y > 0) {
		entities[id]->rigidBody3D.Teleport(entities[id]->getSpawnPoint());
	}
}

void GameMap::LoadInteractable(std::unique_ptr<InteractableObject> interactable)
{
	auto id = interactable->id;
	interactables[id] = std::move(interactable);
	interactables[id]->onEnable();
}

// Unload Object From Scene GameMap
void GameMap::DestroyGameObject(uint64_t id) {
	auto it = gameObjects.find(id);
	if (it == gameObjects.end()) return;

	// ~GameObject() is trivial and never frees GPU resources — release a
	// generated fallback model/mesh here or it leaks the moment this object
	// is erased.
	it->second.releaseGeneratedModel();

	gameObjects.erase(it);
}

// Remove Entity From Entity List
void GameMap::DestroyEntity(uint64_t id)
{
	// Entities live only in Scene::entities, not in gameObjects (see SpawnEntity),
	// so there is nothing to remove from gameObjects here.
	auto it = entities.find(id);
	if (it == entities.end()) return;
	// See DestroyGameObject() — the unique_ptr erase below destroys the Entity
	// without ever freeing its generated model.
	it->second->releaseGeneratedModel();
	entities.erase(it);
}

// Remove Interactable From InteractableList
void GameMap::DestroyInteractable(uint64_t id)
{
	auto it = interactables.find(id);
	if (it == interactables.end()) return;

	auto* object = it->second.get();

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
	interactables.erase(it);
}

GameObject* GameMap::FindGameObject(uint64_t id)
{
	auto it = gameObjects.find(id);
	if (it == gameObjects.end()) { return nullptr; }
	return &it->second;
}

Entity* GameMap::FindEntity(uint64_t id)
{
	auto it = entities.find(id);
	if (it == entities.end()) { return nullptr; }
	return it->second.get();
}

InteractableObject* GameMap::FindInteractable(uint64_t id)
{
	auto it = interactables.find(id);
	if (it == interactables.end()) { return nullptr; }
	return it->second.get();
}

InteractableObject* GameMap::FindRunningStation()
{
	InteractableObject* result = nullptr;
	ForEachInteractable([&](InteractableObject& interactable) {
		if (interactable.isRunningMiniGame) { result = &interactable; return false; }
		return true;
	});
	return result;
}

GameObject* GameMap::FindWorldObject(uint64_t id)
{
	// Interactables Hold Pointers to Activator Objects, making them the first ones required.
	if (auto* interactable = FindInteractable(id)) { return interactable; }
	if (auto* entity = FindEntity(id)) { return entity; }
	return FindGameObject(id);
}
