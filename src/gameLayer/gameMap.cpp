#include "gameMap.h"

#include <asserts.h>
#include <randomStuff.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

#include <entt/entity/registry.hpp>

namespace GameMapStorage
{
	struct Plain
	{
		static constexpr bool in_place_delete = true;
		GameObject object;
	};

	struct EntityKind
	{
		static constexpr bool in_place_delete = true;
		std::unique_ptr<Entity> entity;
	};

	struct Interactable
	{
		static constexpr bool in_place_delete = true;
		std::unique_ptr<InteractableObject> interactable;
	};
}

struct GameMap::Storage
{
	entt::registry registry;
	std::unordered_map<std::uint64_t, entt::entity> byId; // the World save's ids (E-1) to registry entities
	std::size_t gameObjects = 0;
	std::size_t entities = 0;
	std::size_t interactables = 0;
};

GameMap::GameMap() : storage(std::make_unique<Storage>()) {}
GameMap::~GameMap() = default;
GameMap::GameMap(GameMap&&) noexcept = default;
GameMap& GameMap::operator=(GameMap&&) noexcept = default;

namespace
{
	// Stores an object under its id: a new registry entity with its kind's component
	template <class Component, class Value>
	Component& Store(entt::registry& registry, std::unordered_map<std::uint64_t, entt::entity>& byId, std::uint64_t id, Value&& value)
	{
		const entt::entity handle = registry.create();
		byId[id] = handle;
		return registry.emplace<Component>(handle, std::forward<Value>(value));
	}

	template <class Component>
	Component* Stored(entt::registry& registry, const std::unordered_map<std::uint64_t, entt::entity>& byId, std::uint64_t id)
	{
		const auto it = byId.find(id);
		if (it == byId.end()) { return nullptr; }
		return registry.try_get<Component>(it->second);
	}
}

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
	return storage->byId.contains(id);
}

void GameMap::Remove(std::uint64_t id)
{
	const auto it = storage->byId.find(id);
	if (it == storage->byId.end()) { return; }
	entt::registry& registry = storage->registry;
	if (registry.all_of<GameMapStorage::Plain>(it->second)) { --storage->gameObjects; }
	if (registry.all_of<GameMapStorage::EntityKind>(it->second)) { --storage->entities; }
	if (registry.all_of<GameMapStorage::Interactable>(it->second)) { --storage->interactables; }
	registry.destroy(it->second);
	storage->byId.erase(it);
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
	GameObject& object = Store<GameMapStorage::Plain>(storage->registry, storage->byId, id, GameMapStorage::Plain{ prototype }).object;
	++storage->gameObjects;
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
	Store<GameMapStorage::EntityKind>(storage->registry, storage->byId, id, GameMapStorage::EntityKind{ std::move(owned) });
	++storage->entities;
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
	InteractableObject* interactable = Store<GameMapStorage::Interactable>(storage->registry, storage->byId, id, GameMapStorage::Interactable{ std::make_unique<InteractableObject>(prototype) }).interactable.get();
	++storage->interactables;
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
	GameObject& stored = Store<GameMapStorage::Plain>(storage->registry, storage->byId, id, GameMapStorage::Plain{ std::move(object) }).object;
	++storage->gameObjects;
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
	Entity* stored = Store<GameMapStorage::EntityKind>(storage->registry, storage->byId, id, GameMapStorage::EntityKind{ std::move(entity) }).entity.get();
	++storage->entities;
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
	InteractableObject* stored = Store<GameMapStorage::Interactable>(storage->registry, storage->byId, id, GameMapStorage::Interactable{ std::move(interactable) }).interactable.get();
	++storage->interactables;
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

	storage->registry.clear();
	storage->byId.clear();
	storage->gameObjects = 0;
	storage->entities = 0;
	storage->interactables = 0;
	instanceHolder = InstanceID{};
}

GameObject* GameMap::FindGameObject(uint64_t id)
{
	auto* stored = Stored<GameMapStorage::Plain>(storage->registry, storage->byId, id);
	return stored ? &stored->object : nullptr;
}

Entity* GameMap::FindEntity(uint64_t id)
{
	auto* stored = Stored<GameMapStorage::EntityKind>(storage->registry, storage->byId, id);
	return stored ? stored->entity.get() : nullptr;
}

InteractableObject* GameMap::FindInteractable(uint64_t id)
{
	auto* stored = Stored<GameMapStorage::Interactable>(storage->registry, storage->byId, id);
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

size_t GameMap::GameObjectCount() const { return storage->gameObjects; }
size_t GameMap::EntityCount() const { return storage->entities; }
size_t GameMap::InteractableCount() const { return storage->interactables; }

// Each Visit* walks the registry view directly, as the ForEach templates did, so
// the order and the behaviour when the callback spawns or destroys are unchanged.
void GameMap::VisitAllObjects(GameMapDetail::FunctionRef<void(GameObject&)> visit)
{
	entt::registry& registry = storage->registry;
	for (auto [handle, stored] : registry.view<GameMapStorage::Plain>().each()) { visit(stored.object); }
	for (auto [handle, stored] : registry.view<GameMapStorage::EntityKind>().each()) { visit(*stored.entity); }
	for (auto [handle, stored] : registry.view<GameMapStorage::Interactable>().each()) { visit(*stored.interactable); }
}

void GameMap::VisitGameObjects(GameMapDetail::FunctionRef<bool(GameObject&)> visit)
{
	for (auto [handle, stored] : storage->registry.view<GameMapStorage::Plain>().each())
	{
		if (!visit(stored.object)) return;
	}
}

void GameMap::VisitGameObjectsConst(GameMapDetail::FunctionRef<bool(const GameObject&)> visit) const
{
	for (auto [handle, stored] : storage->registry.view<const GameMapStorage::Plain>().each())
	{
		const GameObject& object = stored.object;
		if (!visit(object)) return;
	}
}

void GameMap::VisitEntities(GameMapDetail::FunctionRef<bool(Entity&)> visit)
{
	for (auto [handle, stored] : storage->registry.view<GameMapStorage::EntityKind>().each())
	{
		if (!visit(*stored.entity)) return;
	}
}

void GameMap::VisitEntitiesConst(GameMapDetail::FunctionRef<bool(const Entity&)> visit) const
{
	for (auto [handle, stored] : storage->registry.view<const GameMapStorage::EntityKind>().each())
	{
		const Entity& entity = *stored.entity;
		if (!visit(entity)) return;
	}
}

void GameMap::VisitInteractables(GameMapDetail::FunctionRef<bool(InteractableObject&)> visit)
{
	for (auto [handle, stored] : storage->registry.view<GameMapStorage::Interactable>().each())
	{
		if (!visit(*stored.interactable)) return;
	}
}

void GameMap::VisitInteractablesConst(GameMapDetail::FunctionRef<bool(const InteractableObject&)> visit) const
{
	for (auto [handle, stored] : storage->registry.view<const GameMapStorage::Interactable>().each())
	{
		const InteractableObject& interactable = *stored.interactable;
		if (!visit(interactable)) return;
	}
}
