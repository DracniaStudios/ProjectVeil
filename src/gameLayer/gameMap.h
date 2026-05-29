#pragma once
#ifndef GAMEMAP_H
#define GAMEMAP_H

#include <raylib.h>
#include <vector>
#include <GameObject.h>

struct GameMap {

	std::vector<GameObject> gameObjects = {};
	int objectID = 0;

	Vector3 size = {10, 10, 10};
	void create(Vector3 size);

	// Alter Data
	GameObject& saveObjectAt(Vector3 position, GameObject& object);
	GameObject& saveObjectAt(int x, int y, int z, GameObject& object);

	// Return Data
	Vector3 getMapSize() {	return Vector3(size.x, size.y, size.z);}

};

#endif
