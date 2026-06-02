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

	floor.rigidBody3D->isStatic = true;
	floor.rigidBody3D->translation = Vector3(0, -1, 0);
	floor.rigidBody3D->scale = size;
	floor.defaultColor = BLACK;

	saveObjectAt(floor.getPosition(), floor);

}

GameObject& GameMap::saveObjectAt(Vector3 position, GameObject& object)
{
	// Usus copy ctor to create a new instance of the object
	// Then modify its position and add it to the game map
	auto obj = std::make_unique<GameObject>(object);

	/// Set RigidBody3D Data
	obj->rigidBody3D->translation = position;
	if (obj->rigidBody3D->scale == Vector3Zero())
	{
		obj->rigidBody3D->scale = Vector3One();
	}

	/// Set GameMap Data
	obj->onEnable();
	gameObjects.push_back(std::move(obj));
	gameObjects.back()->rigidBody3D->owner = gameObjects.back().get();

	std::cout << "Added Object \n";
	return *gameObjects.back();
}

GameObject& GameMap::saveObjectAt(int x, int y, int z, GameObject& object)
{
	return saveObjectAt(Vector3(x, y, z), object);
}

std::vector<GameObject*> removalQueue;

void GameMap::removeObject(GameObject* object) {
    if (!object) return;
    removalQueue.push_back(object);
}

void GameMap::flushRemovals() {
    if (removalQueue.empty()) return;
    std::erase_if(SceneManager::getInstance().currentScene->gameMap.gameObjects, [&](const std::unique_ptr<GameObject>& o){
        return std::find(removalQueue.begin(), removalQueue.end(), o.get()) != removalQueue.end();
    });
    removalQueue.clear();
}
