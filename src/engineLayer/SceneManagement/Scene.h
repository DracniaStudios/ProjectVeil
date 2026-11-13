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
#include <sim/Slots.h>
#include <sim/Rules.h>
#include <sim/GameEvents.h>
#include <sim/StationTable.h>

#include <array>

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
		MiniGameSession miniGameSession; // no factory: the client's StartMiniGame installs one

		// Perception — the only channel through which AI learns about the player.
		// Owned by the Scene because emitters and listeners are both scene objects.
		SoundField soundField = {};

		// Applies pressure from world facts only. Never reads player position.
		Director director = {};

		/** Players **/
		// Every player in the scene, by Slot; an empty Slot is nullptr. Slot 0 is
		// built with the scene, and a Host adds the others as they join. The
		// simulation knows players only by Slot, never as "local" (rule R2): the
		// client's own player is Client::LocalPlayer(scene).
		std::array<Player*, Sim::kMaxPlayers> players = {};

		// Builds the player for an empty Slot at its spawn point and enables it.
		// Returns the player already there if the Slot is taken, or nullptr for a
		// Slot outside 0..kMaxPlayers-1.
		Player* AddPlayer(int slot);

		// Frees a Slot's player, with the artifact it carries. Nothing for an empty Slot.
		void RemovePlayer(int slot);

		// The player in a Slot, or nullptr
		Player* PlayerInSlot(int slot) const { return Sim::IsValidSlot(slot) ? players[slot] : nullptr; }

		// The player with this object id, or nullptr if no player has it
		Player* FindPlayer(std::uint64_t id) const;

		int PlayerCount() const;

		// Applies fn(slot, player) to every player, in Slot order
		template <class F>
		void ForEachPlayer(F&& fn)
		{
			for (int slot = 0; slot < Sim::kMaxPlayers; ++slot)
			{
				if (players[slot] != nullptr) { fn(slot, *players[slot]); }
			}
		}

		// Where the scene's players spawn. Each Slot stands a little apart from
		// the last, so players who join together don't spawn inside one another.
		Vector3 SpawnPointFor(int slot) const;

		// Moves the scene's spawn point, and every player to its own spawn
		void SetSpawnPoint(Vector3 point);
		Vector3 GetSpawnPoint() const { return spawnPoint; }

		// The flashlight state last handed to the LightingSystem, so Scene_frame
		// only applies the local player's switch when it changes. Client state.
		bool drawnFlashlight = false;

		// Seconds the Jumpscare stays up (Client::UpdatePresentation). Client state.
		float jumpscareSeconds = 0.0f;

		/** Task stations: the server's half (rules S-1 to S-7) **/
		// The World save's Task stations, the only truth for completion (S-1)
		Sim::StationTable stations;

		// Ticks this scene has simulated; a claim's minimum time is counted in them
		std::uint32_t simTick = 0;

		// The World save's content hash (World::ContentHash), taken as it loads
		std::uint64_t contentHash = 0;

		// Builds the StationTable and the content hash from the World save just
		// loaded. Called by SaveSystem::LoadWorld.
		void OnWorldLoaded();

		// The table index of a station id, or -1. A station the table doesn't know
		// yet (placed in the World Editor) rebuilds the table.
		int StationIndexOf(std::uint64_t stationId);

		/**
		 * A Slot asks for a Task station (S-3). Granted when the station is in the
		 * table, not completed and not held, the Slot's player is close enough and
		 * holds no other station, and the unlock gate passes. A grant makes the
		 * activation noise and starts the minimum-time count. Either way a
		 * StationEvent goes to the outbox.
		 */
		bool ClaimStation(int index, int slot);

		// Only the holder can Complete, and not sooner than the station's minimum
		// seconds after the grant (S-3, S-4). Completing applies the reward to the
		// holder (S-5) and derives the Targets (S-6).
		bool CompleteStation(int index, int slot);

		// Only the holder can Release. Nothing for anyone else.
		bool ReleaseStation(int index, int slot);

		// Frees whatever a Slot holds: on disconnect, catch or wipe (S-4)
		void ReleaseStationsHeldBy(int slot, bool report);

		/** MiniGames: the client's half **/
		// Starts the MiniGame screen for a Task station the server granted, or a
		// Practice run with none, and opens its overlay. False, with nothing
		// changed, for an id that names no game.
		bool StartMiniGame(int gameId, InteractableObject* station = nullptr, Player* by = nullptr);

		// Runs the MiniGame for a rendered frame while it is the top overlay. A win
		// at a station asks the server to Complete it; a Practice win changes
		// nothing (S-5). Per frame, not per tick: the MiniGame is the client's
		// screen, and it reads the player's keys the way a menu does.
		void TickMiniGame(float deltaTime);

		// Ends the MiniGame unwon and closes its overlay, and asks the server to
		// Release the station. Progress stays in the history.
		void QuitMiniGame();

		// The World Editor's buttons: a fresh round, and a win sent like a real one
		void RetryMiniGame();
		void WinMiniGame();

		// What this client asks of the stations, for its connection to send
		std::vector<Sim::StationRequest> stationRequests;
		std::vector<Sim::StationRequest> TakeStationRequests() { std::vector<Sim::StationRequest> taken; taken.swap(stationRequests); return taken; }

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

		/** Escape (rule C-3, Q61) **/
		// Records an escape for every enabled player inside an unlocked Exit. Runs
		// after the catches, so a catch in the same tick wins (Sim::EvaluateRound).
		void CheckEscapes();

		/** The round **/
		// The World save this scene was built from, as authored (SaveSystem::LoadWorld),
		// and its name. The Full wipe restores from it; the content hash reads it.
		Json authoredWorld;
		std::string worldName;

		// +1 every time a round ends (rule C-1). A client seeing a new epoch drops
		// the round it was in at once (rule P-8).
		std::uint32_t roundEpoch = 0;

		// Set once a round ends in an escape: the round is over until the scene is
		// rebuilt or wiped, so no later catch or escape counts.
		bool roundWon = false;

		// Facts for the clients, collected during the tick: PlayerCaught,
		// RoundOver and StationEvent. The server sends and empties it every tick;
		// a scene with no server running just drops them (TakeEvents).
		std::vector<Sim::GameEvent> outbox;
		std::vector<Sim::GameEvent> TakeEvents() { std::vector<Sim::GameEvent> taken; taken.swap(outbox); return taken; }

		/**
		 * The Full wipe (rule C-2, CONTEXT.md): every World-save object goes back to
		 * its authored state under its own id, every player to spawn defaults, the
		 * Director's memory and the sound field are cleared, the stalker is back to
		 * Patrol and any MiniGame is closed. Not a Clear: ids are kept.
		 */
		void FullWipe();

		// One player back to how it spawned: at its spawn point, full health, no
		// buffs, no artifact, looking down +Z
		void ResetPlayerToSpawn(int slot);
	private:
		// A won MiniGame: a station's asks the server to Complete it
		void FinishWonMiniGame(const MiniGameResult& won);

		// Closes the MiniGame overlay
		void CloseMiniGame();

		// Reports a StationEvent
		void ReportStation(Sim::StationEventKind kind, int index, int slot);

		// Countdown to the running station's next noise. Lives on the Scene rather
		// than on the station because the station is a plain world object that never
		// ticks — the minigame it launched is what is actually running.
		float stationNoiseTimer = 0.0f;

		// See CaughtThisTick
		std::vector<std::uint64_t> caughtThisTick;

		// Where Slot 0 spawns; see SpawnPointFor
		Vector3 spawnPoint = { 0, 5, 2 };

		// Players inside an unlocked Exit this tick
		std::vector<std::uint64_t> escapedThisTick;

	public:
		// Decides the round once, at the end of the tick, from the catches and
		// escapes the tick collected (rule C-1): reports each catch, and on any
		// outcome but Ongoing reports RoundOver, Full-wipes after a catch, and
		// moves roundEpoch on. Called by Scene_tick.
		void ResolveRound();
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
	void Scene_tick(Scene& scene, float delta, std::span<const Sim::PlayerCommand> commands = {},
	                bool simulationFrozen = false);

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