#pragma once
#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include <vector>
#include <string>
struct Scene;

namespace SaveSystem
{

	inline std::string saveName = "Default Save";
	inline std::vector<std::string> game_saves = {};

	// Save/Load the full Scene to a named file
	bool SaveGame(const char* fileName, Scene* scene);
	bool LoadGame(const char* fileName, Scene& scene);

	// Save/Load the default world file (falls back to backup on load)
	bool SaveWorld(void* data);
	bool LoadWorld(Scene& scene);

}
#endif
