#include "gameMap.h"

#include <iostream>
#include <string>

void GameMap::create(Vector3 size)
{
	this->size = size;

	GameObject floor = {};
	floor.canBeSelected = false;
	floor.isDestructable = false;

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

void GameMap::removeObject(GameObject* object)
{
	if (object == nullptr) { return; }
	
	std::erase_if(gameObjects, [object](const GameObject& o) { return o.id == object->id; });

	//std::erase(gameObjects, *object);
}

void GameMap::removeObject(int id)
{
	if (id <= 0) { return; }

	std::erase_if(gameObjects, [id](const GameObject& o) { return o.id == id; });

	/* Alternative Scan
	
	for (auto it = gameObjects.begin(); it != gameObjects.end(); ++it)
	{
		if (it->id == id)
		{
			gameObjects.erase(it);
			return;
		}
	}
	*/
}
