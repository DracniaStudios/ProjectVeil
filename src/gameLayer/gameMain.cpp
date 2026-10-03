#include "gameMain.h"

#include <cmath>

constexpr float maxDeltaTime = 1.0f / 30.0f; // 30 FPS

bool init_game()
{
	Settings::Settings::getInstance().Init();
	AudioManager::AudioManager::getInstance().loadAll();
	InitAudioDevice();

	AssetManager::AssetManager::getInstance().loadAll();

	// Lighting must come up AFTER AssetManager (so the shader can be written
	// into every shared asset model) but BEFORE SceneManager_init, which
	// constructs the Main Menu scene and loads its save file. Objects created
	// while the system was still down would keep raylib's unlit shader and
	// render half-lit with no error anywhere.
	LightingSystem::getInstance().Init();

	SceneManagement::SceneManager_init(&SceneManagement::SceneManager::getInstance());

	// Camera
	SceneManagement::SceneManager::getInstance().camera3D.position = Vector3{ 0, 10, 10 };
	SceneManagement::SceneManager::getInstance().camera3D.target = Vector3{ 0, 0, 0 };
	SceneManagement::SceneManager::getInstance().camera3D.up = Vector3(0.0f, 1.0f, 0.0f);
	SceneManagement::SceneManager::getInstance().camera3D.fovy = 90;
	SceneManagement::SceneManager::getInstance().camera3D.projection = CAMERA_PERSPECTIVE;

	SceneManagement::SceneManager::getInstance().camera2D.zoom = 1.0f;// Scale Screen To World (1 pixel = 1 unit)
	SceneManagement::SceneManager::getInstance().camera2D.rotation = 0.0f;
	SceneManagement::SceneManager::getInstance().camera2D.offset = Vector2{ 0, 0 };
	SceneManagement::SceneManager::getInstance().camera2D.target = Vector2{ 0, 0 };

	// Go To Main Menu
	SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), SCENE_MAIN_MENU);
	
	return true;
}

bool update_game()
{
	/// Limit Frame Rate
	const float deltaTime = fminf(GetFrameTime(), maxDeltaTime);

	// Nothing draws a sky, so the clear colour *is* the horizon. Clearing to the
	// fog colour lets distant geometry dissolve into the background instead of
	// fading toward a bright wall.
	ClearBackground(LightingSystem::getInstance().fogColor);

	// Update Input System
	InputSystem::InputSystem::getInstance().Update();

	/// Update and Draw Scene
	SceneManagement::SceneManager_update(&SceneManagement::SceneManager::getInstance(), deltaTime);

	SceneManagement::SceneManager_draw(&SceneManagement::SceneManager::getInstance());
	
	DrawFPS(10, 10);
	return true;
}

void close_game()
{
	std::cout << "Close Game \n";
	std::ofstream f(RESOURCES_PATH "debug.log");


	auto now = std::chrono::system_clock::now();
	std::string date_str = "backup/" + std::format("{:%Y-%m-%d}", now) + "_backup";

	SaveSystem::SaveGame(date_str.c_str(), SceneManagement::SceneManager::getInstance().currentScene);
	
	
	f << "\n CLOSED\n";
	f.close();

	// Materials only hold a copy of the Shader struct, so every model's shader
	// id dangles once this runs — it has to be the last engine call. ProjectVeil.cpp
	// invokes close_game() before CloseWindow(), while the GL context is alive.
	LightingSystem::getInstance().Shutdown();
}