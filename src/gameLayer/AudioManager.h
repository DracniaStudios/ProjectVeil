#pragma once
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <fmod_studio.hpp>
#include <fmod.hpp>
#include <string>
#include <unordered_map>
#include <GameObject.h>

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

	void Play(const std::string& name,float volume = 1.0f);
	void Play3D(const std::string& name, GameObject& object, float volume = 1.0f);
	void PlayEvent(const std::string& eventPath);
	void PlayEvent3D(const std::string& eventPath, GameObject& object);

private:
	FMOD::System* system = nullptr;
	std::unordered_map<std::string, FMOD::Sound*> sounds;
	FMOD::Studio::System* studioSystem = nullptr;
	std::vector<FMOD::Studio::Bank*> banks;
};

#endif
