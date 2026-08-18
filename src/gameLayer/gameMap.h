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

	/**
	 * Where the player is placed when this world loads.
	 *
	 * Nothing positioned the player on a world load before this: LoadWorld
	 * rebuilt the geometry and left the player wherever it already was, which
	 * on a fresh launch is the RigidBody3D default of (0,0,0). In chunk_1 that
	 * is the middle of the artifact display — Artifact Display Floor sits at
	 * the origin, Pedastool at (0,1,0) and Pillar at (0,-10,0) — so every
	 * session began with the player embedded in the plinth, which read as the
	 * level having loaded extra geometry on top of them.
	 *
	 * hasSpawnPoint distinguishes "authored to be the origin" from "the file
	 * predates spawn points", so an older world is left alone rather than being
	 * teleported to a origin that was never chosen.
	 */
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
