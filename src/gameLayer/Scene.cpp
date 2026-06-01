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
		for (auto& bodyA : scene->gameMap.gameObjects)
		{
			//permaAssertComment(&bodyA == nullptr, "Null bodyA @ Scene.cpp");
			for (auto& bodyB : scene->gameMap.gameObjects)
			{
				//permaAssertComment(&bodyB == nullptr, "Null bodyB @ Scene.cpp");

				if (CheckCollisionBoxes(bodyA.rigidBody3D.collisionBox, bodyB.rigidBody3D.collisionBox))
				{
					bodyA.rigidBody3D.resolveConstrains(&bodyB.rigidBody3D);
				}
			}

			// Refresh the collision box after each correction so subsequent
			// iterations use the updated position rather than the stale one
			bodyA.rigidBody3D.collisionBox = {
				Vector3Subtract(bodyA.rigidBody3D.translation, Vector3Scale(bodyA.rigidBody3D.scale, 0.5f)),
				Vector3Add(bodyA.rigidBody3D.translation,      Vector3Scale(bodyA.rigidBody3D.scale, 0.5f))
			};
		}
	}
}

void Scene_updateScene(void* manager_ptr, Scene* scene, float delta) {
	scene->update(manager_ptr, scene->object_ptr, delta);

	// Update GameObjects
	for (auto& object : scene->gameMap.gameObjects) {
		object.update(scene, delta);
	}

	solveCollision(scene, delta, 8);
}

void Scene_drawScene2D(void* manager_ptr, Scene* scene) {
	scene->draw2D(manager_ptr, scene->object_ptr);
}

void Scene_drawScene3D(void* manager_ptr, Scene* scene) {
	scene->draw3D(manager_ptr, scene->object_ptr);
}