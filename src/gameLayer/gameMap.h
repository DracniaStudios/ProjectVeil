#pragma once
#ifndef GAMEMAP_H
#define GAMEMAP_H

#include <raylib.h>
#include <vector>
#include <Entity.h>

inline constexpr int OBJECT_LIMIT = 10000;

struct InstanceID
{
	std::uint64_t idCounter = 2;
	std::uint64_t getIdAndIncrement();
};

struct GameMap {

	std::vector<GameObject> gameObjects = {};
	std::unordered_map<std::uint64_t, std::unique_ptr<Entity>> entities{}; // Entities
	std::unordered_map<std::uint64_t, std::unique_ptr<InteractableObject>> interactables{}; // Interactables
	InstanceID instanceHolder = {}; // All IDs Stored

	Vector3 size = {10, 10, 10};
	
	// PLayer Spawn Point on Load Scene
	Vector3 spawnPoint = {0, 0, 0};
	bool hasSpawnPoint = false;

	void create(Vector3 size);

	// Alter Data
	GameObject* saveObject(GameObject& object);
	Entity* saveEntity(Entity& object);
	InteractableObject* saveInteractable(InteractableObject& object);

	// Remove Data (Change to References)
	void removeObject(GameObject* object);
	void removeEntity(Entity* entity);
	void removeInteractable(InteractableObject* object);

	// Return Data
	Vector3 getMapSize() const { return Vector3(size.x, size.y, size.z);}
};

// Find GameObjects
inline GameObject* FindGameObjectByID(GameMap& gameMap, uint64_t id)
{

	for (size_t i = 0; i < gameMap.gameObjects.size(); ++i) {
		// Must bind by reference — copying the element and returning its
		// address hands the caller a pointer to a stack temporary that is
		// gone the instant this function returns.
		auto& obj = gameMap.gameObjects[i];
		if (obj.id == id) { return &obj; }
	}
	return nullptr;
};

inline Entity* FindEntityByID(GameMap& gameMap, uint64_t id)
{
	
	auto it = gameMap.entities.find(id);
	if (it == gameMap.entities.end()) { return nullptr; }
	return it->second.get();
};

inline InteractableObject* FindInteractableByID(GameMap& gameMap, uint64_t id)
{
	
	auto it = gameMap.interactables.find(id);
	if (it == gameMap.interactables.end()) { return nullptr; }
	return it->second.get();
};

// The task station whose minigame is currently running, or nullptr for none.
//
// Scanned rather than cached: isRunningMiniGame is the flag the Director, the
// station noise emitter and the editor panel already read, and a second copy of
// the same fact would be one more thing to keep in step across
// activate/release/replay. It lives here rather than on Scene so that
// MiniGame.h's CompleteMiniGame — which is handed a GameMap and never a Scene —
// can reach it to mark the station done.
inline InteractableObject* FindRunningStation(GameMap& gameMap)
{
	for (auto& [id, interactable] : gameMap.interactables)
	{
		if (interactable && interactable->isRunningMiniGame) { return interactable.get(); }
	}
	return nullptr;
};

inline GameObject* FindWorldObjectByID(GameMap& gameMap, uint64_t id)
{

	// Interactables Hold Pointers to Activator Objects, making them the first ones required.
	if (auto* interactable = FindInteractableByID(gameMap, id)) { return interactable; }
	if (auto* entity = FindEntityByID(gameMap, id)) { return entity; }
	return FindGameObjectByID(gameMap, id);
};

#endif
