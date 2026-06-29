#pragma once
#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H
#include <vector>

namespace SaveSystem
{

	inline const char* saveName;
	inline std::vector<const char*> game_saves = {};


	void Init();
	void Update();

	bool SaveGame(void* data);
	bool LoadGame(void* data);

	bool SaveWorld(void* data);
	bool LoadWorld(void* data);

	bool SaveGameObjectsOnly(void* data);
	bool LoadGameObjectsOnly(void* data);

	bool SaveSettings(void* data);
	bool LoadSettings(void* data);

}
#endif