#pragma once

#ifndef SCENE_H_
#define SCENE_H_

#include <raylib.h>
#include <raymath.h>
#include <gameMap.h>
#include <imgui.h>
#include <randomStuff.h>

#include <Player.h>
#include <PlayerCamera.h>

#include <stdlib.h>
#include <iostream>
#include <string>
#include <asserts.h>


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
typedef void (*drawSceneMethod2D)(void* manager_ptr, void* object_ptr);

typedef void (*drawSceneMethod3D)(void* manager_ptr, void* object_ptr);

/**
 * Struct to represent a Scene
 */
typedef struct Scene {
	const char* name = {};
	void* object_ptr; // Reference Scene
	updateSceneMethod update;
	drawSceneMethod2D draw2D;
	drawSceneMethod3D draw3D;
	GameMap gameMap; // The GameMap of the Scene
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
void Scene_drawScene2D(void* manager_ptr, Scene* scene);
void Scene_drawScene3D(void* manager_ptr, Scene* scene);

/**
 * Define an construction method for all Scene
 */

#define SCENE_COUNT 1

#define SCENE_MAIN_MENU 0
Scene* Scene_MainMenuConstruct();

#endif 