#include "tutorial.h"

void Scene_TutorialUpdate(SceneManagement::Scene& /*scene*/, float /*deltaTime*/)
{
}

void Scene_TutorialDraw2D(SceneManagement::Scene& /*scene*/)
{
};

void Scene_TutorialDraw3D(SceneManagement::Scene& /*scene*/)
{
	// Weird Interaction Between Rendering Ray and layer Objects

}

SceneManagement::Scene* Scene_TutorialConstruct()
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();

	// Load Main Menu World
	scene->name = "Tutorial";
	scene->update = Scene_TutorialUpdate;
	scene->draw2D = Scene_TutorialDraw2D;
	scene->draw3D = Scene_TutorialDraw3D;

	scene->player->setSpawnPoint(Vector3(20, 2, 0));
	scene->player->rigidBody3D.Teleport(Vector3(20, 2, 0));

	if (!SaveSystem::LoadWorld("chunk_1", *scene)) {
		std::cerr << "[Tutorial] Failed to load world \"chunk_1\"\n";
	}

	return scene;
}