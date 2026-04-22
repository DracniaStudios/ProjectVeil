#include "gameMain.h"

SceneManager manager = {};
Camera3D camera = {};

bool init_game()
{
	InitAudioDevice();
	SceneManager_init(&manager);
	
	// Camera
	camera.position = Vector3{ -5, 5, 0 };
	camera.fovy = 90;
	camera.target = Vector3{ 0, 0, 0 };
	
	return true;
}

bool update_game()
{

	float deltaTime = GetFrameTime();

	return true;
}

void close_game()
{
	std::cout << "Close Game \n";
	std::ofstream f(RESOURCES_PATH "debug.log");

	f << "\n CLOSED\n";
	f.close();
}