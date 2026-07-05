#include "gameMain.h"

bool init_game()
{
	AssetManager::getInstance().loadAll();
	AudioManager::getInstance().loadAll();
	InitAudioDevice();
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

	ClearBackground(WHITE);

	// Update Input System

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

	f << "\n CLOSED\n";
	f.close();
}