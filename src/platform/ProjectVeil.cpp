// ProjectVeil.cpp : Defines the entry point for the application.
//

#include "ProjectVeil.h"

using namespace std;

int main()
{
#if PRODUCTION_BUILD == 1
	std::cout << "Production Build \n";
	SetTraceLogLevel(LOG_FATAL);
#endif

	// Init Settings and Load Data

	//SetConfigFlags(FLAG_WINDOW_MAXIMIZED);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);

	SetTargetFPS(60); // set in settings

	InitWindow(1600, 900, "Project: Veil"); // set in settings

#pragma region ImGui
	rlImGuiSetup(true);
	ImGui_ImplRaylib_Init();
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 2;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.AddMousePosEvent(GetMousePosition().x, GetMousePosition().y);
	io.AddMouseButtonEvent(MOUSE_BUTTON_LEFT, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
	io.AddMouseButtonEvent(MOUSE_BUTTON_RIGHT, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));

	ImGui::StyleColorsDark();
#pragma endregion

#pragma region FMOD

	AudioManager::AudioManager::getInstance().init();

#pragma endregion

	if (!init_game())
	{
		std::cout << "Game Couldn't Load \n";
		return 0;
	}

	SetWindowFocused();
	SetExitKey(KEY_F12);

	while (!WindowShouldClose())
	{
#pragma region ImGui
		rlImGuiBegin();

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {});
		ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, {});
		// Applies Mouse Data to ImGui Window
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::PopStyleColor(2);
#pragma endregion
		BeginDrawing();

		if (IsKeyPressed(KEY_F11)) { ToggleFullscreen(); }
		
		// Dev override for the cursor lock. Read back from raylib rather than kept
		// here: update_game() also locks and frees the cursor as overlays open and close.
		if (IsKeyPressed(KEY_GRAVE))
		{
			if (IsCursorHidden())
			{
				EnableCursor();
			}
			else
			{
				DisableCursor();
			}
		}
		AudioManager::AudioManager::getInstance().update(SceneManagement::SceneManager::getInstance().currentScene);

		bool gameRunning = update_game();

		ImGui::Render();

		ImGui_ImplRaylib_RenderDrawData(ImGui::GetDrawData());
		rlImGuiEnd();
		EndDrawing();

		if (!gameRunning) { break; }
	}

	AudioManager::AudioManager::getInstance().shutdown();

	//ImGui_ImplRaylib_Shutdown();
	rlImGuiShutdown();

	close_game();

	CloseWindow();

	return 0;
}
