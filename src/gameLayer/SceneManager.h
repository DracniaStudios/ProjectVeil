#pragma once

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <stdlib.h>
#include <vector>
#include <Scene.h>
#include <Transition.h>

typedef struct SceneManager {
	Scene* scenes[255]; /**< The Scene list */
	Scene* currentScene; /**< The current Scene active */
	Scene* nextScene; /**< The next Scene to activate */
	Transition* transition; /**< The Transition between two Scene */

	Camera3D camera3D;
	Camera2D camera2D;

} SceneManager;

/**
 * Create a SceneManager and return the pointer
 * @return A pointer to the SceneManager
 */
SceneManager* SceneManager_new();

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


#endif // SCENEMANAGER_H