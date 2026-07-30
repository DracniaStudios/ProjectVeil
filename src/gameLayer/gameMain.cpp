#include "gameMain.h"

bool init_game()
{
	Settings::getInstance().Init();
	AudioManager::getInstance().loadAll();
	InitAudioDevice();

	AssetManager::getInstance().loadAll();

	// Lighting must come up AFTER AssetManager (so the shader can be written
	// into every shared asset model) but BEFORE SceneManager_init, which
	// constructs the Main Menu scene and loads its save file. Objects created
	// while the system was still down would keep raylib's unlit shader and
	// render half-lit with no error anywhere.
	LightingSystem::getInstance().Init();

	SceneManager_init(&SceneManager::getInstance());

	// Camera
	SceneManager::getInstance().camera3D.position = Vector3{ 0, 10, 10 };
	SceneManager::getInstance().camera3D.target = Vector3{ 0, 0, 0 };
	SceneManager::getInstance().camera3D.up = Vector3(0.0f, 1.0f, 0.0f);
	SceneManager::getInstance().camera3D.fovy = 90;
	SceneManager::getInstance().camera3D.projection = CAMERA_PERSPECTIVE;

	SceneManager::getInstance().camera2D.zoom = 1.0f;// Scale Screen To World (1 pixel = 1 unit)
	SceneManager::getInstance().camera2D.rotation = 0.0f;
	SceneManager::getInstance().camera2D.offset = Vector2{ 0, 0 };
	SceneManager::getInstance().camera2D.target = Vector2{ 0, 0 };

	// Go To Main Menu
	SceneManager_push(&SceneManager::getInstance(), SCENE_MAIN_MENU);
	
	return true;
}

bool update_game()
{
	float deltaTime = GetFrameTime();

	// Nothing draws a sky, so the clear colour *is* the horizon. Clearing to the
	// fog colour lets distant geometry dissolve into the background instead of
	// fading toward a bright wall.
	ClearBackground(LightingSystem::getInstance().fogColor);

	// Update Input System
	InputSystem::getInstance().Update();

	/// Update and Draw Scene
	SceneManager_update(&SceneManager::getInstance(), deltaTime);

	SceneManager_draw(&SceneManager::getInstance());
	
	DrawFPS(10, 10);
	return true;
}

void close_game()
{
	std::cout << "Close Game \n";
	std::ofstream f(RESOURCES_PATH "debug.log");
	SaveSystem::SaveGame("backup", SceneManager::getInstance().currentScene);
	f << "\n CLOSED\n";
	f.close();

	// Materials only hold a copy of the Shader struct, so every model's shader
	// id dangles once this runs — it has to be the last engine call. ProjectVeil.cpp
	// invokes close_game() before CloseWindow(), while the GL context is alive.
	LightingSystem::getInstance().Shutdown();
}