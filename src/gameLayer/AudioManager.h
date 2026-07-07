#pragma once
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <fmod_studio.hpp>
#include <fmod.hpp>
#include <string>
#include <unordered_map>

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

	void Play(const std::string& name, float volume = 1.0f);
	void PlayEvent(const std::string& eventPath);

private:
	FMOD::System* system = nullptr;
	std::unordered_map<std::string, FMOD::Sound*> sounds;
	FMOD::Studio::System* studioSystem = nullptr;
	std::vector<FMOD::Studio::Bank*> banks;
};

#endif
