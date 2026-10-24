#pragma once
#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include <functional>
#include <string>
#include <vector>
#include <SaveStore.h>

namespace SceneManagement { struct Scene; }
struct GameObject;

/**
 * Saves a Scene and loads it back, through one pipeline for both kinds of save.
 * See CONTEXT.md for Game save and World save.
 *
 *   Game   the moment the player left: the world, projectiles in flight, the
 *          scene's flags and the player
 *   World  the level as authored in the World Editor: the world and its
 *          lighting, with no player and nothing in flight
 *
 * Loading checks the save first, then Clears the world and Restores every object
 * in it. A save that is turned away (missing, not JSON, written by a newer game,
 * or with a section of the wrong shape) leaves the world as it was.
 */
namespace SaveSystem
{
	inline std::string saveName = "Default Save";
	inline std::vector<std::string> game_saves = {};

	enum class SaveScope { Game, World };

	// A part of the save held outside the Scene, written under its own key.
	// Lighting is one: it is level data, but the LightingSystem holds it.
	struct SaveParticipant
	{
		std::string key;
		std::function<Json()> snapshot;
		// Given the participant's section, or nullptr when the save has none
		std::function<void(const Json* section)> apply;
	};

	// The LightingSystem's lights and atmosphere. A save from before lighting
	// existed gets the default rig, since an unlit world reads as a broken build.
	SaveParticipant LightingParticipant();

	// Writes the scene to the store under `name`. False, with nothing written,
	// when the world holds more than OBJECT_LIMIT objects or the store can't keep it.
	bool Save(SaveStore& store, const std::string& name, SceneManagement::Scene& scene,
		SaveScope scope, const std::vector<SaveParticipant>& participants);

	// Replaces the scene's world with the save under `name`; the player survives
	// a World load. False, with the world untouched, when the save is turned away.
	bool Load(const SaveStore& store, const std::string& name, SceneManagement::Scene& scene,
		SaveScope scope, const std::vector<SaveParticipant>& participants);

	// The game's own saves, lit by the LightingSystem: Game saves under saves/,
	// World saves under resources/scenes/
	bool SaveGame(const char* fileName, SceneManagement::Scene* scene);
	bool LoadGame(const char* fileName, SceneManagement::Scene& scene);
	bool SaveWorld(std::string fileName, SceneManagement::Scene* scene);
	bool LoadWorld(std::string fileName, SceneManagement::Scene& scene);

	std::vector<std::string> GetSaveFiles();
}
#endif
