#pragma once

#ifndef SCENE_H_
#define SCENE_H_

#include <raylib.h>
#include <raymath.h>
#include <imgui.h>
#include <randomStuff.h>

#include <gameMap.h>
#include <InputSystem.h>
#include <Player.h>
#include <Perception/SoundField.h>
#include <AI/Director.h>
#include <AssetManager.h>
#include <MiniGame.h>
#include <asserts.h>
#include <SaveSystem.h>
#include <Settings.h>

#include <stdlib.h>
#include <iostream>
#include <string>
#include <unordered_map>

/**
 * Define an update method
 * @param object_ptr The object_ptr contained in Scene Object
 * @param delta The current deltaTime
 */
typedef void (*updateSceneMethod)(float delta);

/**
 * Define an draw method
 * @param object_ptr The object_ptr contained in Scene Object
 */
typedef void (*drawSceneMethod2D)();

typedef void (*drawSceneMethod3D)();

/**
 * Struct to represent a Scene
 */

 // Force Player ID
constexpr static std::uint64_t PLAYER_ID = 1;

typedef struct Scene {
	
public:
	// Details
	const char* name = "Scene";

	// Flags
	bool is2DActive = false; // 2D Mode
	bool isMiniActive = false;// Mini Game 
	bool limitYBounds = false; // Limit Y Bounds

	// Global Functions
	updateSceneMethod update;
	drawSceneMethod2D draw2D;
	drawSceneMethod3D draw3D;

	// Map Data
	GameMap gameMap = {}; // The GameMap of the Scene
	MiniGame* miniGame = {}; // Current Mini Game Loaded

	// Perception — the only channel through which AI learns about the player.
	// Owned by the Scene because emitters and listeners are both scene objects.
	SoundField soundField = {};

	// Applies pressure from world facts only. Never reads player position.
	Director director = {};
	
	// Entity Data
	Player* player = {};

	void SetMiniGame(int miniGame);
	int GetLastMiniGame() const { return lastMiniGamePlayed; }
	void ReleaseMiniGame(); // Frees MiniGame Memory Data
	void ResetMiniGame();

	// The task station whose minigame is running, or nullptr for none.
	InteractableObject* GetRunningStation();

	// Emits the running station's periodic noise into the sound field. Called
	// once per frame from Scene_updateScene, ahead of the entity updates.
	void EmitStationNoise(float deltaTime);

	void ResetID();
private:
	int lastMiniGamePlayed = 0;

	// Countdown to the running station's next noise. Lives on the Scene rather
	// than on the station because the station is a plain world object that never
	// ticks — the minigame it launched is what is actually running.
	float stationNoiseTimer = 0.0f;
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
void Scene_updateScene(float delta);

/**
 * Draw the current Scene active
 * @param scene The Scene to draw
 */
void Scene_drawScene2D();
void Scene_drawScene3D();

/**
 * Define an construction method for all Scene
 */

inline constexpr int SCENE_COUNT = 1;

inline constexpr int SCENE_MAIN_MENU = 0;
Scene* Scene_MainMenuConstruct();

#endif 