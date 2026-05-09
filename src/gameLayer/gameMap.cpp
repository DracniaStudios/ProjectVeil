#include "gameMap.h"

#include <iostream>
#include <string>

void GameMap::create(Vector3 size)
{
	this->size = size;

	GameObject floor;
	floor.name = "Floor";
	floor.rigidBody3D.isStatic = true;
	floor.canBeSelected = false;
	floor.rigidBody3D.translation = Vector3(0, -1, 0);
	floor.rigidBody3D.scale = size;
	floor.defaultColor = BLACK;

	saveObjectAt(floor.getPosition(), floor);

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

	if (object.rigidBody3D.scale == Vector3Zero())
	{
		object.rigidBody3D.scale = Vector3One();
	}
	object.id = objectID++;

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