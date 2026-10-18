#include "mainMenu.h"

#include <SaveSystem.h>

void Scene_MainMenuUpdate(float /*deltaTime*/)
{



}

void Scene_MainMenuDraw2D()
{
	// The menu itself is the Title overlay, drawn with the other overlays (UI/Overlays.cpp)
};

void Scene_MainMenuDraw3D()
{
	// Weird Interaction Between Rendering Ray and layer Objects

}

SceneManagement::Scene* Scene_MainMenuConstruct()
{
	SceneManagement::Scene* scene = SceneManagement::Scene_new();

	// Load Main Menu World
	scene->name = "Main Menu";
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;


	scene->player->setSpawnPoint(Vector3(0, 2, 0));
	scene->player->rigidBody3D.Teleport(Vector3(0, 2, 0));

	// The Title holds the player's input for as long as this scene lives, so the
	// world runs behind it but the player and their camera never move.
	scene->overlays.Open(Overlay::Title);

	// Fix the camera where the player's camera would sit: at eye height over the
	// spawn point, looking down +Z. Without this the menu shows whatever view the
	// previous scene left in SceneManager::camera3D.
	constexpr float kEyeHeight = 1.0f; // Half the 2-tall body Player::update3D gives the player
	const Vector3 eye = Vector3Add(scene->player->getSpawnPoint(), Vector3(0, kEyeHeight, 0));
	Camera3D& camera = SceneManagement::SceneManager::getInstance().camera3D;
	camera.position = eye;
	camera.target = Vector3Add(eye, Vector3(0, 0, 1));

	if (!SaveSystem::LoadWorld("mainMenu", *scene))
	{
		std::cerr << "[MainMenu] Failed to load the mainMenu world.\n";
	}


	AudioManager::AudioManager::getInstance().Play("Emotion_2", AudioManager::AUDIO_MUSIC);

	return scene;
}
