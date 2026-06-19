#include "scene.h"
#include "SceneManager.h"

Scene* Scene_new() {
	Scene* scene = new Scene;
	SceneManager::getInstance().currentScene = scene;
	return scene;
}

/** Unique Id Instancing **/
std::uint64_t InstanceID::getIdAndIncrement()
{
	std::uint64_t id = idCounter;
	idCounter++;

	permaAssertComment(id < UINT64_MAX - 1, "We ran out of ids somehow...");

	return id;
}

/** Physics Solutions **/
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
					bodyA.rigidBody3D.resolveConstrains(&bodyA, &bodyB);
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

/** Scene Functions **/
void Scene_updateScene(float delta) {

	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	scene->update(delta);

	/** Update GameObjects **/
	for (auto entity = scene->entities.begin(); entity != scene->entities.end();)
	{
		// Update Data
		entity->second->id = entity->first;
		
		bool shouldKill = false;

		if (entity->second->health <= 0)
		{
			shouldKill = true;
		}

		if (shouldKill)
		{
			// Check If Item
			//if (entity.second->type != OBJECT_ITEM) { continue; }
			entity = scene->entities.erase(entity);
		}
		else
		{
			entity->second->update(scene, delta);
			++entity;
		}
	}

	for (auto& object : scene->gameMap.gameObjects) {

		object.update(scene, delta);
	}
	solveCollision(scene, delta, 8);

	/** Complete MiniGame **/
	if (auto miniGame = scene->miniGame)
	{
		if (miniGame->data->score == miniGame->data->scoreGoal)
		{
			std::cout << "Completed Mini Game: " << miniGame->name << "\n";
			scene->isMiniActive = false;
			scene->is2DActive = false;
			// Apply Effects and Boosts
			scene->miniGame = nullptr;
		}
	}

}

void Scene_drawScene2D() {
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	if (scene != nullptr) { scene->draw2D(); }
}

void Scene_drawScene3D() {
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	if (scene != nullptr) { scene->draw3D(); }
}