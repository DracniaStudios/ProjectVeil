#include "Scene.h"
#include <Platform.h>

#include <iostream>
#include <random>

#include <WorldEditor.h>
#include <InputSystem.h>
#include <LightingSystem.h>
#include <UIEngine.h>
#include <UI/Overlays.h>
#include <Client/CommandSampler.h>
#include <Client/LocalPlayer.h>
#include <Client/RoundPresenter.h>
#include <Client/StalkerWarnings.h>
#include <Client/WorldDraw.h>
#include <MiniGame.h>

namespace SceneManagement {

	void Scene_frame(Scene& scene, Camera3D& camera, float frameTime) {

		auto worldEditor = &Editor::WorldEditor::getInstance();
		auto inputSystem = &InputSystem::InputSystem::getInstance();

		// Swap Editor and Player Camera. The editor camera ignores overlays, so the
		// Main Menu world can still be flown around under its Title; the player's
		// camera only follows the player while gameplay has the player's input.
		// It looks along the player's simulated look plus the mouse movement no
		// tick has consumed yet, so turning never waits for a tick.
		if (worldEditor->IsEnabled()) {
			worldEditor->editorCamera.Update(&camera);
		}
		else if (scene.overlays.GameplayHasInput()) {
			if (Player* local = Client::LocalPlayer(scene)) {
				const auto& sampler = Client::CommandSampler::getInstance();
				PlayerCamera::PlaceCamera(&camera, local->EyePosition(), sampler.ViewYaw(), sampler.ViewPitch());
			}
			else {
				std::cerr << "No Camera Detected \n";
			}
		}

		// The flashlight follows the local player's own switch, applied only when
		// that changes, so the World Editor's Flashlight checkbox still works in between
		Player* local = Client::LocalPlayer(scene);
		if (local != nullptr && local->flashlightOn != scene.drawnFlashlight) {
			scene.drawnFlashlight = local->flashlightOn;
			Lighting::LightingSystem::getInstance().SetFlashlightEnabled(scene.drawnFlashlight);
		}

		// The Back action backs out of whatever overlay is on top. Backing out of a
		// minigame quits it, so it is ended here: the stack only records that it closed.
		if (inputSystem->IsActionPressed(InputSystem::ACTION_UI_BACK)) {
			if (scene.overlays.BackPressed() == Overlay::MiniGame) {
				scene.QuitMiniGame();
			}
		}

		// Pause opens over gameplay only, and never from inside the World Editor
		if (!worldEditor->IsEnabled() && scene.overlays.GameplayHasInput()
			&& inputSystem->IsActionPressed(InputSystem::ACTION_UI_PAUSE)) {
			scene.overlays.Open(Overlay::Pause);
		}

		/** Update MiniGame **/
		scene.TickMiniGame(frameTime);

		// The Jumpscare comes down on its own once its time is up
		Client::UpdatePresentation(scene, frameTime);

		// The stalker's footsteps and breathing, before it is close enough to catch
		Client::UpdateStalkerWarnings(scene, frameTime);

		// World Editor (includes the developer tool windows)
		worldEditor->update(scene);
	}

	void Scene_drawScene2D(Scene& scene) {
		// Brackets every widget drawn this frame. The frame is kept whole once it
		// ends, so the UI Inspector, which runs in update, reads all of last
		// frame's UI. Widgets reach the screen at endFrame, over anything drawn
		// straight to raylib in between: the dimming lands under the panels it
		// dims, and a MiniGame and the reticle only draw when no panel does.
		const bool editorHasMouse = Editor::WorldEditor::getInstance().CapturesMouse();
		UI::beginFrame(UI::UIEngine::getInstance(), UI::ReadRaylibInput(editorHasMouse));

		if (scene.draw2D) { scene.draw2D(scene); }

		// The dimming and the top overlay's panel, Title through MiniGame
		UI::drawOverlays(scene);

		// Over the overlays, the reticle during gameplay. Inside a MiniGame the
		// player is the game's own cursor, which the game draws.
		if (Player* local = Client::LocalPlayer(scene); local != nullptr && scene.overlays.GameplayHasInput()) { Client::DrawReticle(*local); }

		UI::endFrame();
	}

	void Scene_drawScene3D(Scene& scene) {
		if (Editor::WorldEditor::getInstance().IsEnabled()) { DrawGrid(100.0f, 1.0f); }

		if (scene.draw3D) { scene.draw3D(scene); }

		Client::DrawWorldObjects3D(scene);

		scene.ForEachPlayer([](int, Player& player) { Client::DrawPlayer3D(player); });

		// Other players are a placeholder capsule (Step 4.6); real models,
		// animation and their flashlights are multiplayer-update work
		scene.ForEachPlayer([](int slot, Player& player) {
			if (slot == Client::LocalSlot() || !player.isEnabled) { return; }
			const Vector3 at = player.getPosition();
			const float half = player.isCrouching ? 0.25f : 0.5f;
			const Color colour = player.flashlightOn ? Color{ 230, 220, 160, 255 } : Color{ 150, 160, 175, 255 };
			DrawCapsule(Vector3{ at.x, at.y - half, at.z }, Vector3{ at.x, at.y + half, at.z }, 0.45f, 8, 6, colour);
			DrawLine3D(Vector3{ at.x, at.y + half, at.z }, Vector3Add(Vector3{ at.x, at.y + half, at.z }, Vector3Scale(player.camera.forward, 0.8f)), BLACK);
		});
	}

	/** The MiniGame screen: the client's half of a station's MiniGame **/

	namespace {
		Vector2 WindowSize() { return Vector2{ static_cast<float>(Platform::ScreenWidth()), static_cast<float>(Platform::ScreenHeight()) }; }

		// The player's input this frame, as the MiniGame reads it
		MiniGameInput ReadMiniGameInput()
		{
			auto& input = InputSystem::InputSystem::getInstance();
			const auto keys = [&input](auto query) {
				return MiniGameKeys{
					(input.*query)(InputSystem::ACTION_MOVE_LEFT), (input.*query)(InputSystem::ACTION_MOVE_RIGHT),
					(input.*query)(InputSystem::ACTION_MOVE_FORWARD), (input.*query)(InputSystem::ACTION_MOVE_BACKWARD),
					(input.*query)(InputSystem::ACTION_MOVE_JUMP), (input.*query)(InputSystem::ACTION_MOVE_SPRINT) };
			};

			MiniGameInput read;
			read.held = keys(&InputSystem::InputSystem::IsActionDown);
			read.pressed = keys(&InputSystem::InputSystem::IsActionPressed);
			read.screenSize = WindowSize();
			return read;
		}
	}

	bool Scene::StartMiniGame(int gameId, InteractableObject* station, Player* by)
	{
		if (by == nullptr) { by = PlayerInSlot(0); }

		// A Scene is built without a factory (headless); the screen that starts a game supplies it
		if (!miniGameSession.HasFactory()) { miniGameSession.SetFactory(CreateMiniGame); }

		const std::uint64_t stationId = station != nullptr ? station->id : 0;
		if (!miniGameSession.Start(gameId, stationId, WindowSize(), std::random_device{}()))
		{
			std::cout << "[Scene.cpp] Ignoring a MiniGame start with unknown id: " << gameId << "\n";
			return false;
		}

		// The screen only: which station is held is the server's (StationTable)
		if (by != nullptr) { by->artifactMode = gameId; }
		overlays.Open(Overlay::MiniGame);
		return true;
	}

	void Scene::TickMiniGame(float deltaTime)
	{
		// A game under another overlay waits: it can't be lost while it can't be seen
		if (overlays.Top() != Overlay::MiniGame || !miniGameSession.IsRunning()) { return; }

		const MiniGameResult result = miniGameSession.Tick(ReadMiniGameInput(), deltaTime);
		if (result.outcome == MiniGameOutcome::Won) { FinishWonMiniGame(result); }
	}

	void Scene::QuitMiniGame()
	{
		const std::uint64_t stationId = miniGameSession.StationId();
		const bool running = miniGameSession.IsRunning();
		miniGameSession.Quit();
		CloseMiniGame();

		// Walking away from a station gives it back
		if (running && stationId != 0)
		{
			const int index = stations.IndexOf(stationId);
			if (index >= 0) { stationRequests.push_back({ Sim::StationAction::Release, index }); }
		}
	}

	void Scene::RetryMiniGame()
	{
		miniGameSession.Retry(WindowSize());
	}

	void Scene::WinMiniGame()
	{
		const MiniGameResult won = miniGameSession.Win();
		if (won.outcome == MiniGameOutcome::Won) { FinishWonMiniGame(won); }
	}

	void Scene::FinishWonMiniGame(const MiniGameResult& won)
	{
		// A Practice run stays on this client and changes nothing on the server
		// (S-5). A station's win is the server's to judge: it asks to Complete.
		if (won.stationId != 0)
		{
			const int index = stations.IndexOf(won.stationId);
			if (index >= 0) { stationRequests.push_back({ Sim::StationAction::Complete, index }); }
		}

		CloseMiniGame();
	}

	void Scene::CloseMiniGame()
	{
		overlays.Close(Overlay::MiniGame);
	}
}
