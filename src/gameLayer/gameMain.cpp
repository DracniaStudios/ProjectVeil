#include "gameMain.h"

#include <ResourcePaths.h>
#include <SaveSystem.h>
#include <WorldEditor.h>
#include <Client/CommandSampler.h>
#include <Client/LocalPlayer.h>
#include <Client/RoundPresenter.h>
#include <Client/Session.h>
#include <Client/DevOptions.h>
#include <Client/GamePresentation.h>
#include <sim/FixedTick.h>
#include <Platform.h>
#include <Render/Render.h>

#include <cmath>
#include <optional>

// The rate frames are drawn at. The simulation runs at Sim::kTickRate whatever
// this is (DedicatedServerPlan.md Step 1.2).
constexpr int maxFrameRate = 60;

// Turns each frame's time into whole simulation ticks
static Sim::FixedTickClock tickClock;

// Counts the ticks this client has sampled a command for
static std::uint32_t clientTick = 0;

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

	if (gameplayHasInput) { Platform::LockCursor(); }
	else { Platform::UnlockCursor(); }
}

bool init_game()
{
	Client::InstallGamePresentation();
	Settings::Settings::getInstance().Init();
	AudioManager::AudioManager::getInstance().loadAll();
	Platform::SetTargetFps(maxFrameRate);
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

	// The client's save participants, installed BEFORE SceneManager_init: the Main
	// Menu scene loads its world save, which restores the lighting through them.
	SaveSystem::SetDefaultParticipants({ SaveSystem::LightingParticipant() });

	SceneManagement::SceneManager_init(&SceneManagement::SceneManager::getInstance());

	// Single-player runs through the same server code as a Host, joined to this
	// client by Loopback: no socket, no SpacetimeDB (S4, MultiplayerSplitPlan.md N2)
	Client::Session::getInstance().StartOffline();

	// Go To Main Menu
	//SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), SCENE_MAIN_MENU);
	
	return true;
}

bool update_game()
{
	const float frameTime = Platform::FrameTime();

	// A scripted dev run ends cleanly, so its log is complete
	if (Client::Dev().quitAfterSeconds > 0.0 && Platform::Time() >= Client::Dev().quitAfterSeconds) { request_exit(); }

	// --log-mouse: one line each time the clock crosses a whole second, sums reset after
	if (Client::Dev().logMouse)
	{
		static double sumDx = 0.0, sumDy = 0.0;
		static long long lastSecond = -1; // -1: not started, so the first line covers a whole second
		const Vector2 delta = Platform::MouseDelta();
		sumDx += delta.x;
		sumDy += delta.y;
		const long long second = static_cast<long long>(Platform::Time());
		if (lastSecond < 0) { lastSecond = second; }
		if (second > lastSecond)
		{
			lastSecond = second;
			std::cout << "[Input] mouse dx " << sumDx << " dy " << sumDy << "\n";
			sumDx = 0.0;
			sumDy = 0.0;
		}
	}

	// Nothing draws a sky, so the clear colour *is* the horizon. Clearing to the
	// fog colour lets distant geometry dissolve into the background instead of
	// fading toward a bright wall.
	Render::BeginFrame(Lighting::LightingSystem::getInstance().fogColor);

	// Update Input System
	InputSystem::InputSystem::getInstance().Update();

	auto* manager = &SceneManagement::SceneManager::getInstance();

	// A joined client's world moves to the Host's latest Snapshots before the
	// camera is placed in it
	auto& session = Client::Session::getInstance();
	session.Frame(*manager, Platform::Time());

	// Input is sampled once per frame into the pending PlayerCommand (Step 1.3).
	// While an overlay or the World Editor has the input, the command is empty.
	auto& sampler = Client::CommandSampler::getInstance();
	if (const auto scene = manager->currentScene) {
		float yaw = 0.0f, pitch = 0.0f;
		if (session.LocalLook(*scene, yaw, pitch)) { sampler.SetLookBase(yaw, pitch); }
		sampler.SampleFrame(scene->overlays.GameplayHasInput() && !Editor::WorldEditor::getInstance().IsEnabled());
	}

	/// Update and Draw Scene. The frame's work (fade, camera, editor) comes first,
	/// so the ticks simulate with the camera the player is now looking through.
	SceneManagement::SceneManager_frame(manager, fminf(frameTime, Sim::kMaxFrameTime));

	// The development multiplayer windows (Join, Network)
	session.DrawUi();

	// Simulation: zero or more fixed ticks this frame, one command each. The
	// session sends it, runs the server's tick (single-player, Host) and reads
	// what came back.
	// The World Editor changes state only in Scene_frame (SceneManager_frame, above),
	// never during a tick, so one read here serves every tick. Keep it below that call.
	const bool editorPausesSimulation = Editor::WorldEditor::getInstance().IsEnabled() && Editor::WorldEditor::getInstance().IsSimulationPaused();
	const int ticks = tickClock.Advance(frameTime);
	for (int tick = 0; tick < ticks; ++tick) {
		Sim::PlayerCommand command = sampler.Consume(++clientTick);
		if (Client::Dev().bot) { command = Client::BotCommand(command.sequence, clientTick); }
		session.Tick(*manager, command, editorPausesSimulation);
	}

	SceneManagement::SceneManager_draw(manager);

	// After the draw too, so a menu button that opened or closed an overlay this
	// frame moves the cursor this frame
	ApplyCursorForInput();

	// raylib's DrawFPS: green, orange under 30, red under 15
	const int fps = Platform::Fps();
	Render::Text(std::format("{:2} FPS", fps), { 10.0f, 10.0f }, 20, fps < 15 ? RED : fps < 30 ? ORANGE : LIME);
	Render::EndFrame();
	return !exitRequested;
}

void close_game()
{
	std::cout << "Close Game \n";
	std::ofstream f(ResourcePaths::Resources() / "debug.log");


	auto now = std::chrono::system_clock::now();
	std::string date_str = "backup/" + std::format("{:%Y-%m-%d}", now) + "_backup";

	SaveSystem::SaveGame(date_str.c_str(), SceneManagement::SceneManager::getInstance().currentScene);
	
	
	f << "\n CLOSED\n";
	f.close();

	// Networking stops before the window and the GL context go (§5.6)
	Client::Session::getInstance().Shutdown();

	// The current scene is freed while GL (and FMOD) are still alive
	SceneManagement::SceneManager_shutdown(&SceneManagement::SceneManager::getInstance());

	// Materials only hold a copy of the Shader struct, so every model's shader
	// id dangles once this runs — it has to be the last engine call. ProjectVeil.cpp
	// invokes close_game() before CloseWindow(), while the GL context is alive.
	Lighting::LightingSystem::getInstance().Shutdown();

#if PRODUCTION_BUILD != 1
	// The frame-time baseline: work is BeginFrame to EndFrame entry, frame is the whole interval
	const Platform::FrameStats stats = Platform::GetFrameStats();
	std::cout << std::format("[Platform] {} frames, work mean {:.3f} ms max {:.3f} ms, frame mean {:.3f} ms max {:.3f} ms",
		stats.frames, stats.meanWorkMs, stats.maxWorkMs, stats.meanFrameMs, stats.maxFrameMs) << std::endl;
#endif
}