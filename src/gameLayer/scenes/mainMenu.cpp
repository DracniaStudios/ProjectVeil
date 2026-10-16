#include "mainMenu.h"

#include <SaveSystem.h>
#include <UIEngine.h>
#include <gameMain.h>

void Scene_MainMenuUpdate(float /*deltaTime*/)
{



}

void Scene_MainMenuDraw2D()
{
	auto manager = &SceneManagement::SceneManager::getInstance();
	auto scene = manager->currentScene;
	if (!scene) { return; }

	// Draw Main Menu
	// Each row stacks under the last inside the panel, so a new button is one more call here.
	UI::beginPanel("Main Menu", Rectangle{}, Rectangle{ 0.4f, 0.3f, 0.2f, 0.4f });
	{
		UI::drawText("Main Menu");

		if (UI::drawButton("Play"))
		{
			std::cout << "Play Button Clicked\n";
			SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), 1);
		}
		if (UI::drawButton("Multiplayer")) {
			std::cout << "Open Multiplayer Menu\n";
			scene->is2DActive = !scene->is2DActive;
			UI::UIEngine::getInstance().isMultiplayerEnabled = !UI::UIEngine::getInstance().isMultiplayerEnabled;
		}
		if (UI::drawButton("Settings"))
		{
			std::cout << "Open Settings Menu\n";
			// Enable Settings Menu
			scene->is2DActive = !scene->is2DActive;
			UI::UIEngine::getInstance().isSettingsEnabled = !UI::UIEngine::getInstance().isSettingsEnabled;
		}
		if (UI::drawButton("Exit"))
		{
			std::cout << "Exit Button Clicked\n";
			request_exit();
		}
	}
	UI::endPanel();

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

	if (!SaveSystem::LoadWorld("mainMenu", *scene))
	{
		std::cerr << "[MainMenu] Failed to load the mainMenu world.\n";
	}


	AudioManager::AudioManager::getInstance().Play("Emotion_2", AudioManager::AUDIO_MUSIC);

	return scene;
}