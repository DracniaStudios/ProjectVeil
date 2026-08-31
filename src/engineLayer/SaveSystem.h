#pragma once
#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include <vector>
#include <string>
struct Scene;
struct GameObject;

namespace SaveSystem
{

	inline std::string saveName = "Default Save";
	inline std::vector<std::string> game_saves = {};

	// Save/Load the full Scene to a named file
	bool SaveGame(const char* fileName, Scene* scene);
	bool LoadGame(const char* fileName, Scene& scene);

	// Save/Load world geometry only (map size + objects + interactables) to the fixed world.json
	bool SaveWorld(std::string fileName, Scene* scene);
	bool LoadWorld(std::string fileName, Scene& scene);

	std::vector<std::string> GetSaveFiles();
}
#endif
