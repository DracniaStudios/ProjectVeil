#pragma once

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <Scene.h>
#include <Transition.h>

namespace SceneManagement {

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
		Scene* currentScene; /**< The current Scene active */
		int nextSceneID = -1; /**< The Scene to build once the screen has faded out (-1 for none) */
		Transition* transition; /**< The Transition between two Scene */

		Camera3D camera3D; // cast Editor/Player -> SceneManager.Camera
		Camera2D camera2D;

		void SetCamera(Camera3D* camera);

	} SceneManager;

	/**
	 * Initialize a SceneManager with all Scene availables
	 * @param manager The SceneManager to initialize
	 */
	void SceneManager_init(SceneManager* manager);

	/**
	 * Update the current Scene active in SceneManager
	 * @param manager The SceneManager currently used in game
	 * @param delta The current deltaTime
	 */
	void SceneManager_update(SceneManager* manager, float delta);

	/**
	 * Draw the current Scene active in SceneManager
	 * @param manager The SceneManager currently used in game
	 */
	void SceneManager_draw(SceneManager* manager);

	/**
	 * Push a new Scene to display
	 * @param manager The SceneManager currently used in game
	 * @param sceneID The Scene to push
	 */
	void SceneManager_push(SceneManager* manager, int sceneID);

	/**
	* Call a transition between scene
	* @param manager The SceneManager currently used in game
	* @param direction The TransitionDirection to animate (IN, OUT, NONE)
	*/
	void SceneManager_transition(SceneManager* manager, TransitionDirection NONE);

}

#endif // SCENEMANAGER_H
