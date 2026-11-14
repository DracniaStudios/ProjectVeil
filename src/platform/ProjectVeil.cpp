// ProjectVeil.cpp : Defines the entry point for the application.
//

#include "ProjectVeil.h"

#include <Client/DevOptions.h>
#include <Render/Render.h>

using namespace std;

int main(int argc, char** argv)
{
	// Dev builds only: --host, --join, --quit-after and the rest (Client/DevOptions.h)
	Client::ParseDevOptions(argc, argv);

#if PRODUCTION_BUILD == 1
	std::cout << "Production Build \n";
#endif

	// Init Settings and Load Data

	Platform::WindowDesc desc; // 1600x900, resizable; the size is meant to come from settings
	desc.minWidth = 1280; desc.minHeight = 720;
	desc.maxWidth = 3840; desc.maxHeight = 2160;
	if (Client::Dev().windowX >= 0) { desc.x = Client::Dev().windowX; desc.y = 60; }
#if PRODUCTION_BUILD == 1
	desc.backendLog = Platform::LogLevel::Fatal;
#endif
	if (!Platform::Init(desc)) { return 1; }
	if (!Render::Init()) { return 1; }

#pragma region ImGui
	EditorGui::Init();
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 2;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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

	Platform::FocusWindow();
	Platform::SetExitKey(Platform::Key::F12);

	while (!Platform::ShouldClose())
	{
		Platform::BeginFrame();

#pragma region ImGui
		EditorGui::NewFrame();

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {});
		ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, {});
		// Applies Mouse Data to ImGui Window
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::PopStyleColor(2);
#pragma endregion

		if (Platform::IsKeyPressed(Platform::Key::F11)) { Platform::ToggleFullscreen(); }
		
		// Dev override for the cursor lock. Read back from raylib rather than kept
		// here: update_game() also locks and frees the cursor as overlays open and close.
		if (Platform::IsKeyPressed(Platform::Key::Grave))
		{
			if (Platform::IsCursorLocked())
			{
				Platform::UnlockCursor();
			}
			else
			{
				Platform::LockCursor();
			}
		}
		AudioManager::AudioManager::getInstance().update(SceneManagement::SceneManager::getInstance().currentScene);

		bool gameRunning = update_game();

		ImGui::Render();
		EditorGui::RenderDrawData();

		// --screenshot-at: once, after a whole frame is drawn but before it is presented
		// (after present the back buffer is undefined and the capture comes out black)
		if (Client::Dev().screenshotAtSeconds > 0.0 && Platform::Time() >= Client::Dev().screenshotAtSeconds)
		{
			Client::Dev().screenshotAtSeconds = 0.0;
			const std::string file = "veil_" + Client::Dev().name + ".png";
			Render::CaptureScreenshot(file);
			std::cout << "[Dev] Screenshot " << file << "\n";
		}

		Platform::EndFrame();

		if (!gameRunning) { break; }
	}

	//ImGui_ImplRaylib_Shutdown();
	EditorGui::Shutdown();

	close_game();

	Render::Shutdown();

	AudioManager::AudioManager::getInstance().shutdown();

	Platform::Shutdown();

	return 0;
}
