// ProjectVeil.cpp : Defines the entry point for the application.
//

#include "ProjectVeil.h"

using namespace std;

static bool lockMouse;
int main()
{
#if PRODUCTION_BUILD == 1
	std::cout << "Production Build \n";
	SetTraceLogLevel(LOG_FATAL);
#endif

	//SetConfigFlags(FLAG_WINDOW_MAXIMIZED);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);

	SetExitKey(KEY_F9);
	SetTargetFPS(60);

	InitWindow(1600, 900, "Project: Veil");

#pragma region ImGui
	rlImGuiSetup(true);

	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 2;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

#pragma endregion

#pragma region FMOD

	AudioManager::getInstance().init();

#pragma endregion

	if (!init_game())
	{
		std::cout << "Game Couldn't Load \n";
		return 0;
	}

	SetWindowFocused();
	SetExitKey(KEY_NULL);
	while (!WindowShouldClose())
	{

#pragma region ImGui
		BeginDrawing();

		rlImGuiBegin();

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {});
		ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, {});
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);
		ImGui::PopStyleColor(2);

#pragma endregion

		if (IsKeyPressed(KEY_F11)) { ToggleFullscreen(); }
		
		if (IsKeyPressed(KEY_GRAVE))
		{
			lockMouse = !lockMouse;
			if (lockMouse)
			{
				EnableCursor();
			}
			else
			{
				DisableCursor();
			}
		}
		AudioManager::getInstance().update();

		bool gameRunning = update_game();

		rlImGuiEnd();
		EndDrawing();

		if (!gameRunning) { break; }
	}

	AudioManager::getInstance().shutdown();

	rlImGuiShutdown();

	close_game();

	CloseWindow();

	return 0;
}
