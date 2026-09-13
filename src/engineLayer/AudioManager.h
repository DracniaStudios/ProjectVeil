#pragma once
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <fmod_studio.hpp>
#include <fmod.hpp>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

// Not <GameObject.h>. That include and GameObject.h's include of this file formed
// a cycle that only survived on the include guards, and it dragged FMOD into 40 of
// the 50 translation units. GameObject appears here only as a pointer member and as
// a reference parameter in declarations, so the forward declaration is enough --
// same as Physics.h does for GameMap and GameObject.
//
// <raylib.h> and <vector> used to arrive through that cycle: Vector3 is taken and
// returned by value by the two inline converters below, and `banks` is a
// std::vector. Both are now included directly, so this header compiles standalone.
struct GameObject;

inline Vector3 FMODToVector3(FMOD_VECTOR vector) {
	Vector3 vec = {};
	vec.x = vector.x;
	vec.y = vector.y;
	vec.z = vector.z;
	return vec;
}
inline FMOD_VECTOR Vector3ToFMOD(Vector3 vector) {
	FMOD_VECTOR vec = {};
	vec.x = vector.x;
	vec.y = vector.y;
	vec.z = vector.z;
	return vec;
}

enum AudioType {
	AUDIO_NONE,
	AUDIO_MASTER,
	AUDIO_MUSIC,
	AUDIO_SFX,
	AUDIO_GAMEPLAY_SFX,
	AUDIO_DIALOGUE,
};

struct AudioDetails {
	std::string soundName = "";
	GameObject* object = nullptr;
	AudioType type = AUDIO_NONE;
};

class AudioManager
{
	AudioManager() = default;
	~AudioManager() = default;
public:

	// Delete, copy, and move functions to prevent duplication
	AudioManager(const AudioManager&) = delete;
	AudioManager& operator=(const AudioManager&) = delete;
	AudioManager(AudioManager&&) = delete;
	AudioManager& operator=(AudioManager&&) = delete;
	static AudioManager& getInstance()
	{
		static AudioManager instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	void init();
	void loadAll();
	void update();
	void shutdown();

	// File Audio
	bool Play(const std::string& name, AudioType type = AUDIO_NONE, float volume = 1.0f);
	bool Play3D(const std::string& name, GameObject& object, AudioType type = AUDIO_NONE, float volume = 1.0f);
	bool Play3D(const std::string& name, Vector3 position, AudioType type = AUDIO_NONE, float volume = 1.0f);
	
	// Bank Audio
	bool PlayEvent(const std::string& eventPath, AudioType type = AUDIO_NONE, float volume = 1.0f);
	bool PlayEvent3D(const std::string& eventPath, GameObject& object, AudioType type = AUDIO_NONE, float volume = 1.0f);
	bool PlayEvent3D(const std::string& eventPath, Vector3 position, AudioType type = AUDIO_NONE, float volume = 1.0f);

private:
	FMOD::System* system = nullptr;
	std::unordered_map<std::string, FMOD::Sound*> sounds;
	FMOD::Studio::System* studioSystem = nullptr;
	std::vector<FMOD::Studio::Bank*> banks;
};

#endif
