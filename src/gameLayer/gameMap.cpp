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
	return byId.contains(id);
}

void GameMap::Remove(std::uint64_t id)
{
	const auto it = byId.find(id);
	if (it == byId.end()) { return; }
	registry.destroy(it->second);
	byId.erase(it);
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
	GameObject& object = Store<GameMapStorage::Plain>(id, GameMapStorage::Plain{ prototype }).object;
	PrepareSpawnedCopy(object, id);

	/// Set GameMap Data
	object.onEnable();

	std::cout << "Added Object \n";
	return &object;
}


Entity* GameMap::SpawnEntity(const Entity& prototype)
{
	if (!prototype.KindMatchesType())
	{
		std::cerr << "[GameMap] Not spawning Entity '" << prototype.name << "': its kind ("
		          << entityKindToString(prototype.kind) << ") is not its class\n";
		return nullptr;
	}

	// The id is chosen before the entity is stored, so the map key always matches
	// entity->id, and a prototype that already carries a real id (a duplicate of
	// a live entity) can't overwrite the entity stored under that id.
	const std::uint64_t id = instanceHolder.getIdAndIncrement();

	// clone() keeps the prototype's dynamic type, so a Stalker stays a Stalker
	std::unique_ptr<Entity> owned = prototype.clone();
	Entity* entity = owned.get();
	Store<GameMapStorage::EntityKind>(id, GameMapStorage::EntityKind{ std::move(owned) });
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
	InteractableObject* interactable = Store<GameMapStorage::Interactable>(id, GameMapStorage::Interactable{ std::make_unique<InteractableObject>(prototype) }).interactable.get();
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
		object.releaseGeneratedModel(); // nothing is bound yet; kept so a caller that bound a model first does not leak it
		return nullptr;
	}

	ReserveIdsBelow(id + 1);
	GameObject& stored = Store<GameMapStorage::Plain>(id, GameMapStorage::Plain{ std::move(object) }).object;
	BindRestored(stored);
	return &stored;
}

Entity* GameMap::RestoreEntity(std::unique_ptr<Entity> entity)
{
	if (!entity->KindMatchesType())
	{
		std::cerr << "[GameMap] Not restoring Entity '" << entity->name << "': its kind ("
		          << entityKindToString(entity->kind) << ") is not its class\n";
		entity->releaseGeneratedModel(); // nothing is bound yet; kept so a caller that bound a model first does not leak it
		return nullptr;
	}

	const std::uint64_t id = entity->id;
	if (IsIdInUse(id))
	{
		std::cerr << "[GameMap] Not restoring Entity " << id << ": another object has that id\n";
		entity->releaseGeneratedModel(); // nothing is bound yet; kept so a caller that bound a model first does not leak it
		return nullptr;
	}

	ReserveIdsBelow(id + 1);
	Entity* stored = Store<GameMapStorage::EntityKind>(id, GameMapStorage::EntityKind{ std::move(entity) }).entity.get();
	BindRestored(*stored);
	return stored;
}

InteractableObject* GameMap::RestoreInteractable(std::unique_ptr<InteractableObject> interactable)
{
	const std::uint64_t id = interactable->id;
	if (IsIdInUse(id))
	{
		std::cerr << "[GameMap] Not restoring Interactable " << id << ": another object has that id\n";
		interactable->releaseGeneratedModel(); // nothing is bound yet; kept so a caller that bound a model first does not leak it
		return nullptr;
	}

	ReserveIdsBelow(id + 1);
	InteractableObject* stored = Store<GameMapStorage::Interactable>(id, GameMapStorage::Interactable{ std::move(interactable) }).interactable.get();
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

void GameMap::SweepDestroyed(SceneManagement::Scene* scene)
{
	// The ids are collected first and each object is removed by id after its
	// onDestroy, so an onDestroy that adds an object can't invalidate the
	// traversal. onDestroy() releases the object's generated model (through
	// onDisable), which the trivial destructor never would.
	std::vector<std::uint64_t> marked;
	ForEachObject([&](GameObject& object) { if (object.pendingDestroy) { marked.push_back(object.id); } });

	for (const std::uint64_t id : marked)
	{
		GameObject* object = FindWorldObject(id);
		if (object == nullptr) { continue; }
		object->onDestroy(scene);
		Remove(id);
	}
}

void GameMap::Clear()
{
	// ~GameObject() never frees GPU resources, so every generated fallback model is
	// released first, or it leaks the moment its object is erased
	ForEachObject([](GameObject& object) { object.releaseGeneratedModel(); });

	registry.clear();
	byId.clear();
	instanceHolder = InstanceID{};
}

GameObject* GameMap::FindGameObject(uint64_t id)
{
	auto* stored = Stored<GameMapStorage::Plain>(id);
	return stored ? &stored->object : nullptr;
}

Entity* GameMap::FindEntity(uint64_t id)
{
	auto* stored = Stored<GameMapStorage::EntityKind>(id);
	return stored ? stored->entity.get() : nullptr;
}

InteractableObject* GameMap::FindInteractable(uint64_t id)
{
	auto* stored = Stored<GameMapStorage::Interactable>(id);
	return stored ? stored->interactable.get() : nullptr;
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
