#pragma once
#ifndef GAMEMAP_H
#define GAMEMAP_H

#include <raylib.h>
#include <vector>
#include <Entity.h>

inline constexpr int OBJECT_LIMIT = 10000;

struct GameMap {

	std::vector<GameObject> gameObjects = {};

	Vector3 size = {10, 10, 10};
	void create(Vector3 size);

	// Alter Data
	GameObject* saveObject(GameObject& object);
	Entity* saveEntity(Entity& object);
	InteractableObject* saveInteractable(InteractableObject& object);

	// Remove Data (Change to References)
	void removeObject(GameObject* object);
	void removeEntity(Entity* entity);
	void removeInteractable(InteractableObject* object);

	// Find Data
	static GameObject* FindGameObjectByID(std::uint64_t id);
	static Entity* FindEntityByID(std::uint64_t id);
	static InteractableObject* FindInteractableByID(std::uint64_t id);

	/**
	 * Resolve an id against every world container, not just gameObjects.
	 *
	 * The three Find*ByID above each search one container, and objects are
	 * distributed across all three: plain objects live in gameMap.gameObjects,
	 * entities in Scene::entities and interactables in Scene::interactables —
	 * an interactable is deliberately never pushed into gameObjects (see
	 * saveInteractable/removeInteractable and the SaveSystem loaders).
	 *
	 * Callers holding an id authored in the editor cannot know which container
	 * the target ended up in, so asking only one of them is a silent miss.
	 * Ids are unique across all three, so the search order is arbitrary.
	 */
	static GameObject* FindWorldObjectByID(std::uint64_t id);

	// Return Data
	Vector3 getMapSize() const { return Vector3(size.x, size.y, size.z);}
};

// Find GameObjects
#endif
