#include "gameMain.h"

#include <WorldEditor.h>

#include <cmath>
#include <optional>

constexpr float maxDeltaTime = 1.0f / 30.0f; // 30 FPS

// Set by request_exit(). update_game() reports it at the end of the frame, so
// the main loop in ProjectVeil.cpp ends normally and close_game() runs once, in
// its usual place, instead of mid-frame from inside a button handler.
static bool exitRequested = false;

void request_exit()
{
	exitRequested = true;
}

// Whether the cursor was last locked for gameplay; empty until the first frame
static std::optional<bool> cursorLockedForGameplay;

// Locks the cursor while gameplay has the player's input, and frees it for any
// overlay or the World Editor. Applied only when that changes, so the backtick
// toggle in ProjectVeil.cpp still works by hand in between.
static void ApplyCursorForInput()
{
	const auto scene = SceneManagement::SceneManager::getInstance().currentScene;
	const bool gameplayHasInput = scene != nullptr
		&& scene->overlays.GameplayHasInput()
		&& !Editor::WorldEditor::getInstance().IsEnabled();

	if (cursorLockedForGameplay == gameplayHasInput) { return; }
	cursorLockedForGameplay = gameplayHasInput;

	if (gameplayHasInput) { DisableCursor(); }
	else { EnableCursor(); }
}

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
	Lighting::LightingSystem::getInstance().Init();

	// Camera
	// Set BEFORE SceneManager_init, which builds the Main Menu scene: that scene
	// places the camera for its Title, and setting these afterwards would move it.
	SceneManagement::SceneManager::getInstance().camera3D.position = Vector3{ 0, 10, 10 };
	SceneManagement::SceneManager::getInstance().camera3D.target = Vector3{ 0, 0, 0 };
	SceneManagement::SceneManager::getInstance().camera3D.up = Vector3(0.0f, 1.0f, 0.0f);
	SceneManagement::SceneManager::getInstance().camera3D.fovy = 90;
	SceneManagement::SceneManager::getInstance().camera3D.projection = CAMERA_PERSPECTIVE;

	SceneManagement::SceneManager::getInstance().camera2D.zoom = 1.0f;// Scale Screen To World (1 pixel = 1 unit)
	SceneManagement::SceneManager::getInstance().camera2D.rotation = 0.0f;
	SceneManagement::SceneManager::getInstance().camera2D.offset = Vector2{ 0, 0 };
	SceneManagement::SceneManager::getInstance().camera2D.target = Vector2{ 0, 0 };

	SceneManagement::SceneManager_init(&SceneManagement::SceneManager::getInstance());

	// Go To Main Menu
	//SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), SCENE_MAIN_MENU);
	
	return true;
}

bool update_game()
{
	/// Limit Frame Rate
	const float deltaTime = fminf(GetFrameTime(), maxDeltaTime);

	// Nothing draws a sky, so the clear colour *is* the horizon. Clearing to the
	// fog colour lets distant geometry dissolve into the background instead of
	// fading toward a bright wall.
	ClearBackground(Lighting::LightingSystem::getInstance().fogColor);

	// Update Input System
	InputSystem::InputSystem::getInstance().Update();

	/// Update and Draw Scene
	SceneManagement::SceneManager_update(&SceneManagement::SceneManager::getInstance(), deltaTime);

	SceneManagement::SceneManager_draw(&SceneManagement::SceneManager::getInstance());

	// After the draw too, so a menu button that opened or closed an overlay this
	// frame moves the cursor this frame
	ApplyCursorForInput();

	DrawFPS(10, 10);
	return !exitRequested;
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
	Lighting::LightingSystem::getInstance().Shutdown();
}