#include "scene.h"


Scene* Scene_new() {
	Scene* scene = new Scene;
	return scene;
}


void solveCollision(Scene* scene, float delta, int solverIterations = 6)
{
	solverIterations = static_cast<int>(Clamp(solverIterations, 4, 8));
	for (int iter = 0; iter < solverIterations; iter++)
	{
		for (int i = 0; i < static_cast<int>(scene->gameMap.gameObjects.size()); i++)
		{
			auto& bodyA = scene->gameMap.gameObjects[i].rigidBody3D;

			for (int j = i + 1; j < static_cast<int>(scene->gameMap.gameObjects.size()); j++)
			{
				auto& bodyB = scene->gameMap.gameObjects[j].rigidBody3D;

				if (CheckCollisionBoxes(bodyA.collisionBox, bodyB.collisionBox))
				{
					bodyA.resolveCollision(bodyB);
				}
			}

			// Refresh the collision box after each correction so subsequent
			// iterations use the updated position rather than the stale one
			scene->gameMap.gameObjects[i].rigidBody3D.collisionBox = {
				Vector3Subtract(bodyA.translation, Vector3Scale(bodyA.scale, 0.5f)),
				Vector3Add(bodyA.translation,      Vector3Scale(bodyA.scale, 0.5f))
			};
		}
	}
}

void Scene_updateScene(void* manager_ptr, Scene* scene, float delta) {
	scene->update(manager_ptr, scene->object_ptr, delta);

	// Update GameObjects
	for (auto& object : scene->gameMap.gameObjects) {
		object.update(delta);
	}

	solveCollision(scene, delta, 8);
}

void Scene_drawScene2D(void* manager_ptr, Scene* scene) {
	scene->draw2D(manager_ptr, scene->object_ptr);
}

void Scene_drawScene3D(void* manager_ptr, Scene* scene) {
	scene->draw3D(manager_ptr, scene->object_ptr);
	for (auto& object : scene->gameMap.gameObjects) {
		object.render3D();
	}
}