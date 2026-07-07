#include "AudioManager.h"

#include <fmod_errors.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

void AudioManager::init()
{
	// Load Studio Sysytem
	FMOD_RESULT result = FMOD::Studio::System::create(&studioSystem);
	if (result != FMOD_OK)
	{
		std::cout << "[AudioManager] FMOD::Studio::System::create failed: " << FMOD_ErrorString(result) << "\n";
		studioSystem = nullptr;
		return;
	}

	result = studioSystem->initialize(32, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr);
	if (result != FMOD_OK)
	{
		std::cout << "[AudioManager] FMOD::Studio::System::init failed: " << FMOD_ErrorString(result) << "\n";
		studioSystem->release();
		studioSystem = nullptr;
	}
	
	// Load Core System
	studioSystem->getCoreSystem(&system);

	// Initialize Bankj
	FMOD::Studio::Bank* bank;
	studioSystem->loadBankFile(RESOURCES_PATH "banks/Master.string.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
	banks.push_back(bank);
}

void AudioManager::loadAll()
{
	if (!system)
	{
		std::cout << "[AudioManager] Skipping sound load, FMOD system isn't initialized \n";
		return;
	}

	namespace fs = std::filesystem;
	
	// Load Audio File By Name
	{
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
	

	// Load Audio By Bank
	{
		const fs::path bankDir = RESOURCES_PATH "audio/banks";

		if (!fs::exists(bankDir)) {
			std::cout << "[Audio Manager] No Bank Directory found at " << bankDir.string() << "\n";
			return;
		}

		for (const auto& entry : fs::directory_iterator(bankDir)) {
			if (!entry.is_regular_file()) continue;

			if (entry.path().extension() != ".bank") continue;

			FMOD::Studio::Bank* bank = nullptr;
			FMOD_RESULT result = studioSystem->loadBankFile(
				entry.path().string().c_str(),
				FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);

			if (result == FMOD_OK) {
				std::cout << "Successfully loaded bank: " << entry.path().string() << "\n";
				banks.push_back(bank);

				// OPTIONAL: Automatically load sample data into memory immediately.
				// If skipped, sample data loads on-demand when an event plays.
				bank->loadSampleData();
			}
			else {
				std::cerr << "Failed to load bank: " << entry.path().string() << " | Error: " << result << "\n";
				continue;
			}

		}
	}
}

void AudioManager::update()
{
	if (studioSystem) studioSystem->update();
}

void AudioManager::shutdown()
{
	for (auto& [name, sound] : sounds) { sound->release(); }
	sounds.clear();

	if (studioSystem)
	{
		studioSystem->unloadAll();
		studioSystem->release();
		studioSystem = nullptr;
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
	//system->playSound(it->second, nullptr, false, &channel);
	if (channel) channel->setVolume(volume);
}

void AudioManager::PlayEvent(const std::string& eventPath) {
	if (!studioSystem) return;

	FMOD::Studio::EventDescription* description = nullptr;
	FMOD_RESULT result = studioSystem->getEvent(eventPath.c_str(),
		&description);

	if (result != FMOD_OK) {
		std::cout << "[Audio Manager] No event \"" << eventPath << "\": " << FMOD_ErrorString(result) << "\n";
		return;
	}

	FMOD::Studio::EventInstance* instance = nullptr;
	description->createInstance(&instance);
	instance->start();
	instance->release(); // Destroy when finished playing sound
}
