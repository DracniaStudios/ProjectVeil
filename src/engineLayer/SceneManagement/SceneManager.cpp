#include "SceneManager.h"

#include <iostream>

#include <LightingSystem.h>
#include <WorldEditor.h>

namespace SceneManagement {
	void SceneManager::SetCamera(Camera3D* camera) { camera3D = *camera; }

	// Builds one fresh instance of a scene. Every case returns: the cases used to
	// fall through, so building the main menu rebuilt the tutorial as well.
	Scene* ConstructScene(int sceneIndex) {
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

		// Not current while it is built: the factory hands the scene to everything
		// that builds it, so it becomes current only once it is whole
		Scene* loaded = manager->factory(sceneID);
		if (loaded == nullptr) {
			std::cerr << "[SceneManager] No scene was built for id " << sceneID << "; keeping the current one\n";
			return;
		}

		Scene* previous = manager->currentScene;
		manager->currentScene = loaded;

		for (int i = 0; i < SCENE_COUNT; i++) {
			if (manager->scenes[i] == previous) { manager->scenes[i] = nullptr; }
		}
		manager->scenes[sceneID] = loaded;

		// Every push builds a fresh scene, so the one it replaces is freed rather
		// than leaked. It is no longer current, and this runs from
		// SceneManager_frame before any scene work, so nothing is iterating it.
		if (previous != nullptr && previous != loaded) { Scene_delete(previous); }
	}

	// Starts one half of the fade, or ends it. In builds the requested scene first,
	// so it is built while the screen is fully dark.
	static void StartFade(SceneManager* manager, SceneFade fade) {
		if (fade == SceneFade::In) { LoadNextScene(manager); }

		manager->fade = fade;
		manager->fadeOpacity = fade == SceneFade::In ? 1.0f : 0.0f;
	}

	void SceneManager_init(SceneManager* manager, SceneFactory factory) {
		manager->currentScene = nullptr;
		manager->nextSceneID = -1;
		manager->factory = factory;
		manager->fade = SceneFade::None;
		manager->fadeOpacity = 0.0f;
		for (int i = 0; i < SCENE_COUNT; i++) { manager->scenes[i] = nullptr; }

		// Scenes are built on demand, one per push, so start-up builds only the
		// main menu.
		SceneManager_push(manager, SCENE_MAIN_MENU);
	}

	void SceneManager_frame(SceneManager* manager, float frameTime) {

		// Update Fade. Measured in time rather than frames, so it lasts as long at
		// 30 frames a second as at 144.
		const float step = frameTime / FADE_SECONDS;
		if (manager->fade == SceneFade::Out) {
			manager->fadeOpacity += step;
			if (manager->fadeOpacity >= 1.0f) { StartFade(manager, SceneFade::In); }
		}
		else if (manager->fade == SceneFade::In) {
			manager->fadeOpacity -= step;
			if (manager->fadeOpacity <= 0.0f) { StartFade(manager, SceneFade::None); }
		}

		if (Scene* scene = manager->currentScene) { Scene_frame(*scene, manager->camera3D); }
	}

	void SceneManager_tick(SceneManager* manager, float delta) {
		if (Scene* scene = manager->currentScene) { Scene_tick(*scene, delta); }
	}

	void SceneManager_draw(SceneManager* manager) {

		auto lighting = &Lighting::LightingSystem::getInstance();
		Scene* scene = manager->currentScene;

		// Lighting runs in three ordered stages, all before the camera pass:
		//   1. Update  - picks the shadow-casting light, drives the flashlight from
		//                the active camera, and uploads every light/atmosphere uniform.
		//   2. Shadow  - retargets the framebuffer to render the depth map, so it
		//                cannot happen inside BeginMode3D.
		//   3. Bind    - binds the finished depth texture for the camera pass.
		lighting->Update(manager->camera3D, GetFrameTime());
		lighting->RenderShadowPass(scene);

		// Draw Scene 3D
		BeginMode3D(manager->camera3D);
		lighting->BindShadowMap();
		if (scene) Scene_drawScene3D(*scene);

		if (Editor::WorldEditor::getInstance().IsEnabled())
		{
			lighting->DrawGizmos();
			if (scene) Editor::WorldEditor::getInstance().DrawViewport3D(*scene);
		}
		EndMode3D();

		// Draw Scene 2D
		BeginMode2D(manager->camera2D);
		if (scene) Scene_drawScene2D(*scene);
		EndMode2D();

		// Draw Fade
		if (manager->fade != SceneFade::None) {
			const float opacity = manager->fadeOpacity < 0.0f ? 0.0f : manager->fadeOpacity > 1.0f ? 1.0f : manager->fadeOpacity;
			DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, static_cast<unsigned char>(opacity * 255.0f) });
		}
	}

	void SceneManager_push(SceneManager* manager, int sceneID) {
		if (sceneID >= 0 && sceneID < SCENE_COUNT) {
			std::cout << "Loading Scene: " << sceneID << "\n";


			// Records the next ID as the Scene is loaded from draw2D button.
			manager->nextSceneID = sceneID;

			AudioManager::AudioManager::getInstance().StopAudio();
			StartFade(manager, manager->currentScene ? SceneFade::Out : SceneFade::In);
			//SceneManager_transition(manager, manager->currentScene ? OUT : IN);
		}
	}
}
