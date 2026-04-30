#include "gameMap.h"

#include <iostream>
#include <string>

void GameMap::create(Vector3 size)
{
	mapX = size.x;
	mapY = size.y;
	mapZ = size.z;

}

void GameMap::create(int x, int y, int z)
{
	create(Vector3(x, y, z));
}

GameObject* GameMap::getObjectAt(Vector3 position)
{
	for (auto& o : gameObjects)
	{
		if (o.transform.translation.x == position.x && o.transform.translation.y == position.y && o.transform.translation.z == position.z)
			return &o;
	}
	return nullptr;
}

GameObject* GameMap::getObjectAt(int x, int y, int z)
{
	return getObjectAt(Vector3(x, y, z));
}

GameObject& GameMap::saveObjectAt(Vector3 position, GameObject object)
{
	for (auto& o : gameObjects)
	{
		if (o.transform.translation.x == position.x && o.transform.translation.y == position.y && o.transform.translation.z == position.z)
		{
			o = object;
			std::cout << "Replace Object \n";
			return o;

		}
	}
	gameObjects.push_back(object);
	return gameObjects.back();
}

GameObject& GameMap::saveObjectAt(int x, int y, int z, GameObject object)
{
	return saveObjectAt(Vector3(x, y, z), object);
}

//Vector3 GameMap::get