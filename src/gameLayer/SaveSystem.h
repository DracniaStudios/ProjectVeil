#pragma once
#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include <vector>
struct Scene;
enum ObjectType : uint8_t;

namespace SaveSystem
{

	inline const char* saveName;
	inline std::vector<const char*> game_saves = {};

	bool SaveGame(const char* fileName, void* data, size_t size);
	bool LoadGame(const char* fileName, void* data, size_t size);

	bool SaveWorld(void* data);
	bool LoadWorld(Scene& scene);

}
#endif