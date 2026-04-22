#pragma once

#ifndef SCENE_H_
#define SCENE_H_


#include <stdlib.h>
#include <raylib.h>


/**
 * Define an update method
 * @param object_ptr The object_ptr contained in Scene Object
 * @param delta The current deltaTime
 */
typedef void (*updateSceneMethod)(void* manager_ptr, void* object_ptr, float delta);


/**
 * Define an draw method
 * @param object_ptr The object_ptr contained in Scene Object
 */
typedef void (*drawSceneMethod)(void* manager_ptr, void* object_ptr);

/**
 * Struct to represent a Scene
 */
typedef struct Scene {
	updateSceneMethod update;
	drawSceneMethod draw;
	void* object_ptr;
} Scene;

/**
 * Create a Scene and return the pointer
 * @return A pointer to the Scene
 */
Scene* Scene_new();

/**
 * Update the current Scene active
 * @param scene The Scene to update
 * @param delta The current deltaTime
 */
void Scene_updateScene(void* manager_ptr, Scene* scene, float delta);

/**
 * Draw the current Scene active
 * @param scene The Scene to draw
 */
void Scene_drawScene(void* manager_ptr, Scene* scene);

/**
 * Define an construction method for all Scene
 */

#define SCENE_COUNT 1

#define SCENE_MAIN_MENU 0
Scene* Scene_MainMenuConstruct();

#endif 