#pragma once
#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H
#include <vector>

struct GameObject;

namespace SaveSystem
{

	inline const char* saveName;
	inline std::vector<const char*> game_saves = {};


	void Init();
	void Update();

	static bool SaveGame(void* data);
	static bool LoadGame(void* data);

	bool SaveWorld(void* data);
	bool LoadWorld(void* data);

	bool SaveGameObjectsOnly(std::vector<GameObject> data, const char* fileName);
	bool LoadGameObjectsOnly(void* data);

	bool SaveSettings(void* data);
	bool LoadSettings(void* data);

}
#endif