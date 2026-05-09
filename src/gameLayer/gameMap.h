#pragma once
#ifndef GAMEMAP_H
#define GAMEMAP_H

#include <raylib.h>
#include <vector>

#include <GameObject.h>

struct GameMap {

	std::vector<GameObject> gameObjects = {};

	Vector3 size = {10, 10, 10};
	int objectID = -1;

	void create(Vector3 size);

	// Read Data
	GameObject* getObjectAt(Vector3 position);
	GameObject* getObjectAt(int x, int y, int z);

	// Alter Data
	GameObject& saveObjectAt(Vector3 position, GameObject& object);
	GameObject& saveObjectAt(int x, int y, int z, GameObject& object);

	// Return Data
	Vector3 getMapSize() {	return size;}

	// Convert Data
	//Vector3 getConverstion(Vector3 space) { return Vector3(0, 0, 0); }
};

#endif
