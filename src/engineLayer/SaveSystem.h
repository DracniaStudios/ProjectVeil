#pragma once
#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include <vector>
#include <string>
namespace SceneManagement { struct Scene; }
struct GameObject;

namespace SaveSystem
{

	inline std::string saveName = "Default Save";
	inline std::vector<std::string> game_saves = {};

	// Save/Load the full Scene to a named file
	bool SaveGame(const char* fileName, SceneManagement::Scene* scene);
	bool LoadGame(const char* fileName, SceneManagement::Scene& scene);

	// Save/Load world geometry only (map size + objects + interactables) to the fixed world.json
	bool SaveWorld(std::string fileName, SceneManagement::Scene* scene);
	bool LoadWorld(std::string fileName, SceneManagement::Scene& scene);

	std::vector<std::string> GetSaveFiles();
}
#endif
