#include "gameMap.h"

#include <iostream>
#include <string>

#include "SceneManager.h"

void GameMap::create(Vector3 size)
{
	this->size = size;

	GameObject floor{};
	floor.name = "Floor";
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
	object.onEnable();
	gameObjects.push_back(object);

	std::cout << "Added Object \n";
	return gameObjects.back();
}

GameObject& GameMap::saveObjectAt(int x, int y, int z, GameObject& object)
{
	return saveObjectAt(Vector3(x, y, z), object);
}


void GameMap::removeObject(GameObject* object) {
    if (!object) return;

	std::erase_if(SceneManager::getInstance().currentScene->gameMap.gameObjects, [&](GameObject& o){
        return &o == object;
    });
}
