#pragma once

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <Scene.h>

namespace SceneManagement {

	/**
	 * Builds a fresh instance of the scene with this id
	 * @param sceneID The Scene to build
	 * @return The built Scene, or nullptr for an id that names none
	 */
	typedef Scene* (*SceneFactory)(int sceneID);

	/**
	 * The game's own scenes: the Main Menu and the Tutorial. Tests hand
	 * SceneManager_init a factory of their own, so switching scenes needs no
	 * world file and no audio.
	 */
	Scene* ConstructScene(int sceneID);

	/**
	 * The fade to black between two Scenes. Out darkens the screen; once it is
	 * fully dark the requested Scene is built, and In brightens it again.
	 */
	enum class SceneFade { None, In, Out };

	// How long each half of the fade takes. It used to step 5 of 255 per frame,
	// which is this long at 60 frames a second and longer at any lower rate.
	inline constexpr float FADE_SECONDS = 51.0f / 60.0f;

	typedef struct SceneManager
	{
	private:
		SceneManager() = default; // Private constructor to prevent instantiation
	public:
		// Delete, copy, and move functions to prevent duplication
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;
		SceneManager(SceneManager&&) = delete;
		SceneManager& operator=(SceneManager&&) = delete;

		// Global access point to the SceneManager instance
		static SceneManager& getInstance() {
			static SceneManager instance; // Guaranteed to be destroyed and instantiated on first use
			return instance;
		}

		/** Scene Manager Data **/
		Scene* scenes[255]; /**< The Scene list; only the active Scene's slot is filled */

		// The Scene being played, or nullptr before the first one is built. Read by
		// the main loop and SceneManager only: everything a Scene runs is handed it.
		Scene* currentScene;

		int nextSceneID = -1; /**< The Scene to build once the screen has faded out (-1 for none) */
		SceneFactory factory = ConstructScene; /**< Builds each Scene a push asks for */

		SceneFade fade = SceneFade::None;
		float fadeOpacity = 0.0f; /**< 0 shows the Scene, 1 is fully black */

		Camera3D camera3D; // cast Editor/Player -> SceneManager.Camera
		Camera2D camera2D;

		void SetCamera(Camera3D* camera);

	} SceneManager;

	/**
	 * Initialize a SceneManager and build the Main Menu
	 * @param manager The SceneManager to initialize
	 * @param factory Builds each Scene a push asks for
	 */
	void SceneManager_init(SceneManager* manager, SceneFactory factory = ConstructScene);

	/**
	 * The current Scene's work for one rendered frame: the fade (which builds the
	 * requested Scene once the screen is dark), then the Scene's camera, Inventory
	 * key and World Editor (Scene_frame). Runs before the frame's tick.
	 * @param manager The SceneManager currently used in game
	 * @param frameTime The time this frame took
	 */
	void SceneManager_frame(SceneManager* manager, float frameTime);

	/**
	 * Run the current Scene's simulation for one tick (Scene_tick)
	 * @param manager The SceneManager currently used in game
	 * @param delta The current deltaTime
	 */
	void SceneManager_tick(SceneManager* manager, float delta);

	/**
	 * Draw the current Scene active in SceneManager
	 * @param manager The SceneManager currently used in game
	 */
	void SceneManager_draw(SceneManager* manager);

	/**
	 * Ask for a Scene. The current one fades out first and keeps running while it
	 * does; the requested one is built once the screen is dark. With no Scene yet
	 * it is built at once. An id that names no Scene is ignored.
	 * @param manager The SceneManager currently used in game
	 * @param sceneID The Scene to push
	 */
	void SceneManager_push(SceneManager* manager, int sceneID);

}

#endif // SCENEMANAGER_H
