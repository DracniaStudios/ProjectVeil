#include "gameMain.h"

SceneManager manager = {};
bool lockMouse = true;

bool init_game()
{
	InitAudioDevice();
	SceneManager_init(&manager);
	AssetManager assetManager = {};
	assetManager.loadAll();
	
	// Camera
	manager.camera3D.position = Vector3{ 0, 10, 10 };
	manager.camera3D.target = Vector3{ 0, 0, 0 };
	manager.camera3D.up = Vector3(0.0f, 1.0f, 0.0f);
	manager.camera3D.fovy = 90;
	manager.camera3D.projection = CAMERA_PERSPECTIVE;

	manager.camera2D.zoom = 1.0f;// Scale Screen To World (1 pixel = 1 unit)
	manager.camera2D.rotation = 0.0f;
	manager.camera2D.offset = Vector2{ 0, 0 };
	manager.camera2D.target = Vector2{ 0, 0 };

	// Go To Main Menu
	SceneManager_push(&manager, SCENE_MAIN_MENU);
	
	return true;
}

bool update_game()
{
	float deltaTime = GetFrameTime();

	ClearBackground(WHITE);

	// Update Input System

	/// Update and Draw Scene
	SceneManager_update(&manager, deltaTime);

	SceneManager_draw(&manager);
	
	if (IsKeyPressed(KEY_F1)) { lockMouse = !lockMouse; }
	/*
	if (lockMouse)
	{
		DisableCursor();
	}
	else
	{
		EnableCursor();
	}
	*/
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