#include "Scene.h"

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
}
