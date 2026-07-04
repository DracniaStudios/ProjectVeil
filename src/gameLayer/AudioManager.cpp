#include "AudioManager.h"

#include <fmod_errors.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

void AudioManager::init()
{
	FMOD_RESULT result = FMOD::System_Create(&system);
	if (result != FMOD_OK)
	{
		std::cout << "[AudioManager] FMOD::System_Create failed: " << FMOD_ErrorString(result) << "\n";
		system = nullptr;
		return;
	}

	result = system->init(512, FMOD_INIT_NORMAL, nullptr);
	if (result != FMOD_OK)
	{
		std::cout << "[AudioManager] FMOD::System::init failed: " << FMOD_ErrorString(result) << "\n";
		system->release();
		system = nullptr;
	}
}

void AudioManager::loadAll()
{
	if (!system)
	{
		std::cout << "[AudioManager] Skipping sound load, FMOD system isn't initialized \n";
		return;
	}

	namespace fs = std::filesystem;
	const fs::path audioDir = RESOURCES_PATH "audio";

	if (!fs::exists(audioDir))
	{
		std::cout << "[AudioManager] No audio directory found at " << audioDir.string() << "\n";
		return;
	}

	static const std::vector<std::string> supportedExtensions = { ".wav", ".mp3", ".ogg", ".flac" };

	for (const auto& entry : fs::directory_iterator(audioDir))
	{
		if (!entry.is_regular_file()) continue;

		std::string extension = entry.path().extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return (char)tolower(c); });

		if (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) == supportedExtensions.end())
			continue;

		const std::string name = entry.path().stem().string();
		const std::string path = entry.path().string();

		FMOD::Sound* sound = nullptr;
		FMOD_RESULT result = system->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &sound);
		if (result != FMOD_OK)
		{
			std::cout << "[AudioManager] Failed to load " << path << ": " << FMOD_ErrorString(result) << "\n";
			continue;
		}

		sounds[name] = sound;
		std::cout << "[AudioManager] Loaded sound \"" << name << "\" from " << path << "\n";
	}
}

void AudioManager::update()
{
	if (system) system->update();
}

void AudioManager::shutdown()
{
	for (auto& [name, sound] : sounds)
	{
		sound->release();
	}
	sounds.clear();

	if (system)
	{
		system->release();
		system = nullptr;
	}
}

void AudioManager::Play(const std::string& name, float volume)
{
	if (!system) return;

	const auto it = sounds.find(name);
	if (it == sounds.end())
	{
		std::cout << "[AudioManager] No sound loaded with name \"" << name << "\"\n";
		return;
	}

	FMOD::Channel* channel = nullptr;
	system->playSound(it->second, nullptr, false, &channel);
	if (channel) channel->setVolume(volume);
}
