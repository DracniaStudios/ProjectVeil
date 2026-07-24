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
	GameObject* FindGameObjectByID(int id);
	Entity* FindEntityByID(int id);
	InteractableObject* FindInteractableByID(int id);

	// Return Data
	Vector3 getMapSize() const { return Vector3(size.x, size.y, size.z);}
};

// Find GameObjects
#endif
