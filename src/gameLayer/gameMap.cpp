#include "gameMap.h"

#include <iostream>
#include <string>

void GameMap::create(Vector3 size)
{
	mapX = size.x;
	mapY = size.y;
	mapZ = size.z;

	static GameObject floor;
	floor.rigidBody3D.isStatic = true;
	floor.rigidBody3D.translation = Vector3(0, -1, 0);
	floor.defaultColor = BLACK;
	floor.rigidBody3D.scale = size;

	saveObjectAt(floor.getPosition(), floor);

}

void GameMap::create(int x, int y, int z)
{
	create(Vector3(x, y, z));
}

GameObject* GameMap::getObjectAt(Vector3 position)
{
	/*
	for (auto& o : gameObjects)
	{
		if (o.rigidBody3D.translation.x == position.x && o.rigidBody3D.translation.y == position.y && o.rigidBody3D.translation.z == position.z)
			return &o;
	}
	*/
	return nullptr;
}

GameObject* GameMap::getObjectAt(int x, int y, int z)
{
	return getObjectAt(Vector3(x, y, z));
}

GameObject& GameMap::saveObjectAt(Vector3 position, GameObject& object)
{
	object.rigidBody3D.translation = position;
	object.rigidBody3D.scale = Vector3{ 1, 1, 1 };
	object.id = objectID++;
	if (object.id == 0) { std::cout << "Object ID is 0, this may cause issues with object management. Consider starting objectID at 1. \n"; }
	
	/*
	for (auto& o : gameObjects)
	{
		if (o.rigidBody3D.translation.x == position.x && o.rigidBody3D.translation.y == position.y && o.rigidBody3D.translation.z == position.z)
		{
			o = object;
			o.onEnable();
			std::cout << "Replace Object \n";
			return o;

		}
	}
	*/

	object.onEnable();
	std::cout << "Added Object \n";
	gameObjects.push_back(object);
	return gameObjects.back();
}

GameObject& GameMap::saveObjectAt(int x, int y, int z, GameObject& object)
{
	return saveObjectAt(Vector3(x, y, z), object);
}

//Vector3 GameMap::get