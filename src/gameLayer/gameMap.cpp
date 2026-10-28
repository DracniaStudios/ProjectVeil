#include "gameMap.h"

#include <asserts.h>
#include <randomStuff.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>
/** Map Data **/

// Create GameMap and Floor Space
void GameMap::create(Vector3 size)
{
	// The editor's New World reuses a populated map, and a new world starts empty
	Clear();
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

void GameMap::ReserveIdsBelow(std::uint64_t nextId)
{
	instanceHolder.idCounter = std::max(instanceHolder.idCounter, nextId);
}

bool GameMap::IsIdInUse(std::uint64_t id) const
{
	return gameObjects.contains(id) || entities.contains(id) || interactables.contains(id);
}

namespace
{
	// Readies a freshly stored copy of a prototype to be enabled. Disabling the copy's model.
	void PrepareSpawnedCopy(GameObject& copy, std::uint64_t id)
	{
		copy.id = id;
		copy.disownModel();
		copy.pendingDestroy = false;

		/// Set RigidBody3D Data
		if (copy.rigidBody3D.scale == Vector3Zero())
		{
			copy.rigidBody3D.scale = Vector3One();
		}
	}

	// What a restored object needs to take part in the world.
	void BindRestored(GameObject& object)
	{
		object.rigidBody3D.collider.SetObjectID(object.id);
		object.rigidBody3D.collider.canCollide = object.isEnabled;
		object.loadVisuals();
		object.rigidBody3D.SyncBroadPhaseBox();
	}
}

// Load Object into Scene (Vector3)
GameObject* GameMap::SpawnGameObject(const GameObject& prototype)
{
	const std::uint64_t id = instanceHolder.getIdAndIncrement();
	GameObject& object = gameObjects.emplace(id, prototype).first->second;
	PrepareSpawnedCopy(object, id);

	/// Set GameMap Data
	object.onEnable();

	std::cout << "Added Object \n";
	return &object;
}


Entity* GameMap::SpawnEntity(const Entity& prototype)
{
	// The id is chosen before the entity is stored, so the map key always matches
	// entity->id, and a prototype that already carries a real id (a duplicate of
	// a live entity) can't overwrite the entity stored under that id.
	const std::uint64_t id = instanceHolder.getIdAndIncrement();

	// clone() keeps the prototype's dynamic type, so a Stalker stays a Stalker
	std::unique_ptr<Entity> owned = prototype.clone();
	Entity* entity = owned.get();
	entities[id] = std::move(owned);
	PrepareSpawnedCopy(*entity, id);

	entity->onEnable();

	entity->setSpawnPoint(entity->getPosition());
	entity->rigidBody3D.Teleport(entity->getSpawnPoint());

	std::cout << "Added Entity \n";
	return entity;
}

InteractableObject* GameMap::SpawnInteractable(const InteractableObject& prototype)
{
	const std::uint64_t id = instanceHolder.getIdAndIncrement();
	InteractableObject* interactable = (interactables[id] = std::make_unique<InteractableObject>(prototype)).get();
	PrepareSpawnedCopy(*interactable, id);

	interactable->onEnable();

	std::cout << "Add Interactable \n";
	return interactable;
}

GameObject* GameMap::RestoreGameObject(GameObject object)
{
	const std::uint64_t id = object.id;
	if (IsIdInUse(id))
	{
		std::cerr << "[GameMap] Not restoring GameObject " << id << ": another object has that id\n";
		object.releaseGeneratedModel(); // the model loadFromJson bound would leak with it
		return nullptr;
	}

	ReserveIdsBelow(id + 1);
	GameObject& stored = gameObjects.emplace(id, std::move(object)).first->second;
	BindRestored(stored);
	return &stored;
}

Entity* GameMap::RestoreEntity(std::unique_ptr<Entity> entity)
{
	const std::uint64_t id = entity->id;
	if (IsIdInUse(id))
	{
		std::cerr << "[GameMap] Not restoring Entity " << id << ": another object has that id\n";
		entity->releaseGeneratedModel();
		return nullptr;
	}

	ReserveIdsBelow(id + 1);
	Entity* stored = (entities[id] = std::move(entity)).get();
	BindRestored(*stored);
	return stored;
}

InteractableObject* GameMap::RestoreInteractable(std::unique_ptr<InteractableObject> interactable)
{
	const std::uint64_t id = interactable->id;
	if (IsIdInUse(id))
	{
		std::cerr << "[GameMap] Not restoring Interactable " << id << ": another object has that id\n";
		interactable->releaseGeneratedModel();
		return nullptr;
	}

	ReserveIdsBelow(id + 1);
	InteractableObject* stored = (interactables[id] = std::move(interactable)).get();
	BindRestored(*stored);
	return stored;
}

bool GameMap::Destroy(std::uint64_t id)
{
	GameObject* object = FindWorldObject(id);
	if (object == nullptr) { return false; }
	object->Destroy();
	return true;
}

namespace
{
	// Erases every marked object in one container. The ids are collected first and
	// each one is erased by key after its onDestroy, so an onDestroy that adds an
	// object can't invalidate the traversal.
	template <class Container, class AsObject>
	void SweepContainer(Container& container, SceneManagement::Scene* scene, AsObject asObject)
	{
		std::vector<std::uint64_t> marked;
		for (auto& [id, stored] : container)
		{
			if (asObject(stored).pendingDestroy) { marked.push_back(id); }
		}

		for (std::uint64_t id : marked)
		{
			auto it = container.find(id);
			if (it == container.end()) { continue; }

			// onDestroy() releases the object's generated model (through onDisable),
			// which the trivial destructor never would
			asObject(it->second).onDestroy(scene);
			container.erase(id);
		}
	}
}

void GameMap::SweepDestroyed(SceneManagement::Scene* scene)
{
	SweepContainer(gameObjects, scene, [](GameObject& object) -> GameObject& { return object; });
	SweepContainer(entities, scene, [](std::unique_ptr<Entity>& entity) -> GameObject& { return *entity; });
	SweepContainer(interactables, scene,
		[](std::unique_ptr<InteractableObject>& interactable) -> GameObject& { return *interactable; });
}

void GameMap::Clear()
{
	// ~GameObject() never frees GPU resources, so every generated fallback model is
	// released first, or it leaks the moment its object is erased
	ForEachObject([](GameObject& object) { object.releaseGeneratedModel(); });

	gameObjects.clear();
	entities.clear();
	interactables.clear();
	instanceHolder = InstanceID{};
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
