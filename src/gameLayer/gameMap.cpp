#include "gameMap.h"

#include <iostream>
#include <string>

void GameMap::create(Vector3 size)
{
	this->size = size;

	GameObject floor = {};
	floor.canBeSelected = false;
	
	floor.meshVariant = MESH_CUBE;
	floor.meshData = Vector4One();

	floor.rigidBody3D.isStatic = true;
	floor.rigidBody3D.translation = Vector3(0, -1, 0);
	floor.rigidBody3D.scale = size;
	floor.defaultColor = BLACK;
	saveObjectAt(floor.getPosition(), floor);

}

GameObject& GameMap::saveObjectAt(Vector3 position, GameObject& object)
{

	/// Set RigidBody3D Data
	object.rigidBody3D.translation = position;
	if (object.rigidBody3D.scale == Vector3Zero())
	{
		object.rigidBody3D.scale = Vector3One();
	}

	/// Set GameMap Data
	objectID++;
	object.id = objectID;

	object.onEnable();
	gameObjects.push_back(object);
	std::cout << "Added Object \n";
	return object;
}

GameObject& GameMap::saveObjectAt(int x, int y, int z, GameObject& object)
{
	return saveObjectAt(Vector3(x, y, z), object);
}