#pragma once
#ifndef GAMEMAP_H
#define GAMEMAP_H

#include <raylib.h>
#include <vector>

#include <GameObject.h>

struct GameMap {

	std::vector<GameObject> gameObjects = {};
	std::vector<RigidBody3D> rigidBodies3D = {};
	//std::vector<RigidBody2D> rigidBodies2D = {};

	int mapX = 5;
	int mapY = 5;
	int mapZ = 5;

	int objectID = 0;

	void create(int x, int y, int z);
	void create(Vector3 size);

	// Read Data
	GameObject* getObjectAt(Vector3 position);
	GameObject* getObjectAt(int x, int y, int z);

	// Alter Data
	GameObject& saveObjectAt(Vector3 position, GameObject object);
	GameObject& saveObjectAt(int x, int y, int z, GameObject object);

	// Return Data
	Vector3 getMapSize() {	return Vector3(mapX, mapY, mapZ);}

	// Convert Data
	//Vector3 getConverstion(Vector3 space) { return Vector3(0, 0, 0); }
};

#endif
