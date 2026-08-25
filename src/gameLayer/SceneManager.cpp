#include "SceneManager.h"

#include <algorithm>

#include <LightingSystem.h>
#include <WorldEditor.h>

SceneManager* SceneManager_new() {
	SceneManager* manager = (SceneManager*)malloc(sizeof(SceneManager));
	return manager;
}

void SceneManager::SetCamera(Camera3D* camera) { camera3D = *camera; }

void SceneManager_init(SceneManager* manager) {
	manager->currentScene = nullptr;
	manager->nextScene = nullptr;

	// ... Initialize other scenes as needed
	manager->scenes[0] = Scene_MainMenuConstruct();

	manager->transition = Transition_new();

	SceneManager_push(manager, SCENE_MAIN_MENU);
}

void SceneManager_update(SceneManager* manager, float delta) {


	// Update Transition
	if (manager->transition->direction != NONE) {
		// Tuned at 60 FPS (5 per call); scaled by delta and multiplied back up
		// by 60 the same way Player::update2D/SetMoveDirection already are, so
		// the fade takes the same wall-clock time regardless of frame rate
		// instead of finishing in a fixed number of frames.
		// Clamped to at least 1 so a very high refresh rate (small delta) can't
		// round the step down to 0 and stall the transition forever.
		int step = std::max(1, static_cast<int>(5.0f * 60.0f * delta));
		if (manager->transition->direction == OUT) {
			manager->transition->opacity += step;
			if (manager->transition->opacity >= 255) SceneManager_transition(manager, IN);

		}
		else {
			manager->transition->opacity -= step;
			if (manager->transition->opacity <= 0) SceneManager_transition(manager, NONE);
		}
	}

	// Update Scene
	if (manager->currentScene) Scene_updateScene(delta);

}

void SceneManager_draw(SceneManager* manager) {

	auto lighting = &LightingSystem::getInstance();

	// Lighting runs in three ordered stages, all before the camera pass:
	//   1. Update  - picks the shadow-casting light, drives the flashlight from
	//                the active camera, and uploads every light/atmosphere uniform.
	//   2. Shadow  - retargets the framebuffer to render the depth map, so it
	//                cannot happen inside BeginMode3D.
	//   3. Bind    - binds the finished depth texture for the camera pass.
	lighting->Update(manager->camera3D, GetFrameTime());
	lighting->RenderShadowPass(manager->currentScene);

	// Draw Scene 3D
	BeginMode3D(manager->camera3D);
	lighting->BindShadowMap();
	if (manager->currentScene) Scene_drawScene3D();

	// Light gizmos are an authoring aid, so they only appear while the World
	// Editor is open. Drawn last because their x-ray mode toggles GL depth
	// testing, which must not straddle the scene's own draw calls.
	//
	// The editor's own overlay (selection outline, transform gizmo, placement
	// ghost) comes after them for the same reason and because it must win the
	// pixel: the handles have to stay visible and grabbable even when the object
	// is buried inside geometry. Both restore the depth state on their way out.
	if (WorldEditor::getInstance().IsEnabled())
	{
		lighting->DrawGizmos();
		WorldEditor::getInstance().DrawViewport3D();
	}
	EndMode3D();

	// Draw Scene 2D
	BeginMode2D(manager->camera2D);
	if (manager->currentScene) Scene_drawScene2D();
	EndMode2D();

	// Draw Transition
	if (manager->transition->direction != NONE)
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, (unsigned char)manager->transition->opacity });
}

void SceneManager_push(SceneManager* manager, int sceneID) {
	if (sceneID >= 0 && sceneID < SCENE_COUNT) {
		manager->nextScene = manager->scenes[sceneID];
		SceneManager_transition(manager, manager->currentScene ? OUT : IN);
	}
}

void SceneManager_transition(SceneManager* manager, TransitionDirection direction) {
	if (direction == IN) {
		manager->currentScene = manager->nextScene;
		manager->nextScene = nullptr;
	}

	manager->transition->direction = direction;

	if (direction == OUT) manager->transition->opacity = 0;
	else if (direction == IN) manager->transition->opacity = 255;
	else manager->transition->opacity = -1;
}