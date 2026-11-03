#pragma once

#ifndef SCENE_H_
#define SCENE_H_

#include <raylib.h>
#include <raymath.h>
#include <imgui.h>
#include <randomStuff.h>

#include <gameMap.h>
#include <PhysicsWorld.h>
#include <InputSystem.h>
#include <Player.h>
#include <OverlayStack.h>
#include <Perception/SoundField.h>
#include <AI/Director.h>
#include <AssetManager.h>
#include <MiniGameSession.h>
#include <asserts.h>
#include <SaveSystem.h>
#include <Settings.h>

#include <sim/PlayerCommand.h>

#include <stdlib.h>
#include <iostream>
#include <span>
#include <string>
#include <unordered_map>


namespace SceneManagement { struct Scene; }

/**
 * A scene's own update, run at the start of each tick
 * @param scene The Scene being ticked
 * @param delta The current deltaTime
 */
typedef void (*updateSceneMethod)(SceneManagement::Scene& scene, float delta);

/**
 * A scene's own drawing, under its objects (3D) or its overlays (2D)
 * @param scene The Scene being drawn
 */
typedef void (*drawSceneMethod2D)(SceneManagement::Scene& scene);

typedef void (*drawSceneMethod3D)(SceneManagement::Scene& scene);

/**
 * Struct to represent a Scene
 */

 // Force Player ID
constexpr static std::uint64_t PLAYER_ID = 1;

namespace SceneManagement {

	typedef struct Scene {

	public:
		// Details
		const char* name = "Scene";

		// Flags
		bool limitYBounds = false; // Limit Y Bounds

		// The Overlays open over this scene, which decide whether gameplay has the
		// player's input. Empty when the scene is built; gone when it is freed.
		OverlayStack overlays = {};

		// Global Functions
		updateSceneMethod update = nullptr;
		drawSceneMethod2D draw2D = nullptr;
		drawSceneMethod3D draw3D = nullptr;

		// Map Data
		GameMap gameMap = {}; // The GameMap of the Scene

		// Resolves collisions between every body in the scene, the player's included,
		// and remembers which pairs are touching. Run through SolveCollisions.
		Physics3D::PhysicsWorld physics = {};

		// The MiniGame being played, if any, and the history of how the last ones went.
		// Start, quit and win through the Scene's methods below, which also keep the
		// MiniGame overlay and the Task station in step with it.
		MiniGameSession miniGameSession = MiniGameSession(CreateMiniGame);

		// Perception — the only channel through which AI learns about the player.
		// Owned by the Scene because emitters and listeners are both scene objects.
		SoundField soundField = {};

		// Applies pressure from world facts only. Never reads player position.
		Director director = {};

		// Entity Data
		Player* player = {};

		// The flashlight state last handed to the LightingSystem, so Scene_frame
		// only applies the local player's switch when it changes. Client state.
		bool drawnFlashlight = false;

		/** MiniGames **/
		// Starts a MiniGame at a Task station, or as a Practice run with none, and
		// opens its overlay. False, with nothing changed, for an id that names no game.
		bool StartMiniGame(int gameId, InteractableObject* station = nullptr);

		// Runs the MiniGame for a rendered frame while it is the top overlay, and
		// applies a win. Per frame, not per tick: the MiniGame is the client's
		// screen, and it reads the player's keys the way a menu does.
		void TickMiniGame(float deltaTime);

		// Ends the MiniGame unwon and closes its overlay. Progress stays in the history.
		void QuitMiniGame();

		// The World Editor's buttons: a fresh round, and a win applied like a real one
		void RetryMiniGame();
		void WinMiniGame();

		// The task station whose minigame is running, or nullptr for none.
		InteractableObject* GetRunningStation();

		// Emits the running station's periodic noise into the sound field. Called
		// once per tick from Scene_tick, ahead of the entity updates.
		void EmitStationNoise(float deltaTime);

		// Empties the GameMap (GameMap::Clear). The physics world forgets its
		// contacts too, since the next world hands the same ids out again.
		void ClearWorld();

		// The collision half of the frame, after every body has moved: resolves every
		// overlapping pair of bodies, the player's included, then applies the game's
		// contact rules (ContactRules). Returns the contacts that started or ended.
		std::vector<Physics3D::ContactEvent> SolveCollisions(int iterations = 8);

		/** Catches **/
		// Raises a catch for every enabled player an enabled stalker is touching or
		// is within its catchRadius of. Runs after the tick's collisions, outside the
		// stalker's FSM, so a catch happens the same way in every state.
		void CheckCatches();

		// Records that a player was caught this tick. Recording the same player twice
		// in one tick is one catch (rule C-1).
		void RaiseCatch(std::uint64_t playerId);

		// The players caught during the current tick, in the order they were caught.
		// Emptied once the tick's round evaluation has run.
		const std::vector<std::uint64_t>& CaughtThisTick() const { return caughtThisTick; }
	private:
		// Grants the won game's reward and, at a Task station, completes it and
		// toggles its Target. Then closes the MiniGame overlay.
		void FinishWonMiniGame(const MiniGameResult& won);

		// Closes the MiniGame overlay and frees every Task station
		void CloseMiniGame();

		// Countdown to the running station's next noise. Lives on the Scene rather
		// than on the station because the station is a plain world object that never
		// ticks — the minigame it launched is what is actually running.
		float stationNoiseTimer = 0.0f;

		// See CaughtThisTick
		std::vector<std::uint64_t> caughtThisTick;

	public:
		// Ends the round for the catches this tick collected, once, at the end of
		// the tick (rule C-1). Called by Scene_tick.
		void ResolveCatches();
	} Scene;

	/**
	 * Create a Scene and return the pointer
	 * @return A pointer to the Scene
	 */
	Scene* Scene_new();

	/**
	 * Free a Scene built by Scene_new, with everything it owns
	 * @param scene The Scene to free; must no longer be the current Scene
	 */
	void Scene_delete(Scene* scene);

	/**
	 * The scene's work for one rendered frame, outside its simulation: the camera
	 * (the World Editor's, or the player's while gameplay has input), the Back and
	 * Pause keys, the local flashlight, the running MiniGame and the World
	 * Editor. Runs before the frame's ticks.
	 * @param scene The Scene being played
	 * @param camera The camera the frame is drawn with
	 * @param frameTime The time this frame took
	 */
	void Scene_frame(Scene& scene, Camera3D& camera, float frameTime);

	/**
	 * The scene's simulation for one tick: its own update, sound, the player and
	 * every object, the Director and collisions. Reads no input device.
	 * @param scene The Scene to tick
	 * @param delta The tick's length (Sim::kTickDelta)
	 * @param commands This tick's command for the player; a player with none
	 *                 is driven by an empty command
	 */
	void Scene_tick(Scene& scene, float delta, std::span<const Sim::PlayerCommand> commands = {});

	/**
	 * Draw a Scene: its world (3D), then its UI and overlays (2D)
	 * @param scene The Scene to draw
	 */
	void Scene_drawScene2D(Scene& scene);
	void Scene_drawScene3D(Scene& scene);
}

/**
 * Define an construction method for all Scene
 */

inline constexpr int SCENE_COUNT = 2;

inline constexpr int SCENE_MAIN_MENU = 0;
SceneManagement::Scene* Scene_MainMenuConstruct();
inline constexpr int SCENE_TUTORIAL = 1;
SceneManagement::Scene* Scene_TutorialConstruct();

#endif 