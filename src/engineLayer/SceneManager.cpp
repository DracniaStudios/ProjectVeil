#include "SceneManager.h"

#include <LightingSystem.h>
#include <WorldEditor.h>

namespace SceneManagement {
	void SceneManager::SetCamera(Camera3D* camera) { camera3D = *camera; }

	// Builds one fresh instance of a scene. Every case returns: the cases used to
	// fall through, so building the main menu rebuilt the tutorial as well.
	static Scene* ConstructScene(int sceneIndex) {
		switch (sceneIndex) {
			case SCENE_MAIN_MENU: return Scene_MainMenuConstruct();
			case SCENE_TUTORIAL: return Scene_TutorialConstruct();
			default: return nullptr;
		}
	}

	// Builds the scene requested by the last push and makes it current. Runs only
	// while nothing of the outgoing scene is on screen: at the fully faded-out
	// midpoint of a transition, or at start-up before any scene exists. Building
	// here rather than in SceneManager_push keeps the outgoing scene running for
	// its whole fade-out, and builds the incoming scene exactly once.
	static void LoadNextScene(SceneManager* manager) {
		const int sceneID = manager->nextSceneID;
		manager->nextSceneID = -1;
		if (sceneID < 0 || sceneID >= SCENE_COUNT) { return; }

		Scene* previous = manager->currentScene;

		// Scene_new() points currentScene at the scene under construction (the
		// world loader relies on it), so currentScene is the new scene from here.
		Scene* loaded = ConstructScene(sceneID);
		manager->currentScene = loaded;

		for (int i = 0; i < SCENE_COUNT; i++) {
			if (manager->scenes[i] == previous) { manager->scenes[i] = nullptr; }
		}
		manager->scenes[sceneID] = loaded;
	}

	void SceneManager_init(SceneManager* manager) {
		manager->currentScene = nullptr;
		manager->nextSceneID = -1;
		for (int i = 0; i < SCENE_COUNT; i++) { manager->scenes[i] = nullptr; }

		manager->transition = Transition_new();

		// Scenes are built on demand, one per push, so start-up builds only the
		// main menu.
		SceneManager_push(manager, SCENE_MAIN_MENU);
	}

	void SceneManager_update(SceneManager* manager, float delta) {


		// Update Transition
		if (manager->transition->direction != NONE) {
			if (manager->transition->direction == OUT) {
				manager->transition->opacity += 5;
				if (manager->transition->opacity >= 255) SceneManager_transition(manager, IN);

			}
			else {
				manager->transition->opacity -= 5;
				if (manager->transition->opacity <= 0) SceneManager_transition(manager, NONE);
			}
		}

		// Update Scene
		if (manager->currentScene) Scene_updateScene(delta);

	}

	void SceneManager_draw(SceneManager* manager) {

		auto lighting = &Lighting::LightingSystem::getInstance();

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

		if (Editor::WorldEditor::getInstance().IsEnabled())
		{
			lighting->DrawGizmos();
			Editor::WorldEditor::getInstance().DrawViewport3D();
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
			// Only record the request. Pushes come from button handlers inside a
			// scene's draw2D, so building or swapping scenes here would happen
			// mid-frame, before the fade-out has even started.
			manager->nextSceneID = sceneID;

			SceneManager_transition(manager, manager->currentScene ? OUT : IN);
		}
	}

	void SceneManager_transition(SceneManager* manager, TransitionDirection direction) {
		if (direction == IN) {
			LoadNextScene(manager);
		}

		manager->transition->direction = direction;

		if (direction == OUT) manager->transition->opacity = 0;
		else if (direction == IN) manager->transition->opacity = 255;
		else manager->transition->opacity = -1;
	}
}