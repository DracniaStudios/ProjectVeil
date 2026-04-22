// ProjectVeil.cpp : Defines the entry point for the application.
//

#include "ProjectVeil.h"

using namespace std;
SceneManager manager = {};
Camera3D camera = {};

int main()
{
#if PRODUCTION_BUILD == 1
	std::cout << "Production Build \n";
	SetTraceLogLevel
#endif

	SetConfigFlags(FLAG_WINDOW_MAXIMIZED);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	SetExitKey(KEY_F10);
	SetTargetFPS(60);

	InitWindow(1600, 900, "Project: Veil");

#pragma region ImGui
	rlImGuiSetup(true);

	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 2;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
#pragma endregion

	if (!init_game())
	{
		std::cout << "Game Couldn't Load \n";
		return 0;
	}

	while (!WindowShouldClose())
	{
		
		BeginDrawing();
		BeginMode3D(camera);
		ClearBackground(BLACK);

#pragma region ImGui
		rlImGuiBegin();

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {});
		ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, {});
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);
		ImGui::PopStyleColor(3);
#pragma endregion

		SceneManager_draw(&manager);

		if (!update_game())
		{
			CloseWindow();
		}
		EndMode3D();
		rlImGuiEnd();
		EndDrawing();
	}

	rlImGuiShutdown();

	CloseWindow();

	close_game();

	return 0;
}
