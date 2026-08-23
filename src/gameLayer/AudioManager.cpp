#include "AudioManager.h"

#include <fmod_errors.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <SceneManager.h>

void updateVolume(float& currentVolume, AudioType type) {

	auto settings = &Settings::getInstance();

	currentVolume *= settings->masterVolume.value;
	switch (type) {
		case AUDIO_MUSIC: currentVolume *= settings->musicVolume.value; break;
		case AUDIO_SFX: currentVolume *= settings->sfxVolume.value; break;
		case AUDIO_GAMEPLAY_SFX: currentVolume *= settings->gameplaySFX.value; break;
		case AUDIO_DIALOGUE: currentVolume *= settings->dialogueVolume.value; break;
		default: break;
	}

}


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
		return;
	}
	
	// Load Core System
	studioSystem->getCoreSystem(&system);
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
			// FMOD_3D so Play3D can spatialize; Play switches its channel back to 2D
			FMOD_RESULT result = system->createSound(path.c_str(), FMOD_3D, nullptr, &sound);
			if (result != FMOD_OK)
			{
				std::cout << "[AudioManager] Failed to load " << path << ": " << FMOD_ErrorString(result) << "\n";
				continue;
			}

			// A duplicate stem name (e.g. two files differing only by extension)
			// would otherwise overwrite the map entry and leak the sound it replaces.
			if (auto existing = sounds.find(name); existing != sounds.end())
			{
				existing->second->release();
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

		// Load Master banks first so events/strings are available to other banks
		const fs::path masterBankPath = bankDir / "Master.bank";
		const fs::path masterStringBankPath = bankDir / "Master.strings.bank";

		auto tryLoadBank = [&](const fs::path& path) {
			if (!fs::exists(path) || !fs::is_regular_file(path)) return;
			FMOD::Studio::Bank* bank = nullptr;
			FMOD_RESULT result = studioSystem->loadBankFile(
				path.string().c_str(),
				FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
			if (result == FMOD_OK) {
				std::cout << "[Audio Manager] Successfully loaded bank: " << path.string() << "\n";
				banks.push_back(bank);

				// OPTIONAL: Automatically load sample data into memory immediately.
				// If skipped, sample data loads on-demand when an event plays.
				bank->loadSampleData();
			}
			else {
				std::cerr << "[Audio Manager] Failed to load bank: " << path.string() << " | Error: " << result << "\n";
			}
		};

		// Attempt to load Master banks first (order: Master.bank, then Master.string.bank)
		tryLoadBank(masterBankPath);
		tryLoadBank(masterStringBankPath);

		for (const auto& entry : fs::directory_iterator(bankDir)) {
			if (!entry.is_regular_file()) continue;

			if (entry.path().extension() != ".bank") continue;

			// Skip the master banks since we've already attempted to load them
			if (entry.path().filename() == masterBankPath.filename() ||
				entry.path().filename() == masterStringBankPath.filename()) continue;

			FMOD::Studio::Bank* bank = nullptr;
			FMOD_RESULT result = studioSystem->loadBankFile(
				entry.path().string().c_str(),
				FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);

			if (result == FMOD_OK) {
				std::cout << "[Audio Manager] Successfully loaded bank: " << entry.path().string() << "\n";
				banks.push_back(bank);

				// OPTIONAL: Automatically load sample data into memory immediately.
				// If skipped, sample data loads on-demand when an event plays.
				bank->loadSampleData();
			}
			else {
				std::cerr << "[Audio Manager] Failed to load bank: " << entry.path().string() << " | Error: " << result << "\n";
				continue;
			}

		}
	}
}

void AudioManager::update()
{
	if (!studioSystem) return;

	const auto scene = SceneManager::getInstance().currentScene;
	if (scene && scene->player)
	{
		FMOD_3D_ATTRIBUTES listener = scene->player->getListener();
		studioSystem->setListenerAttributes(0, &listener);
	}
	studioSystem->update();
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

bool AudioManager::Play(const std::string& name, AudioType type, float volume)
{
	if (!system) return false;

	//

	const auto it = sounds.find(name);
	if (it == sounds.end())
	{
		std::cout << "[AudioManager] No sound loaded with name \"" << name << "\"\n";
		PlayEvent(name, type, volume);
		return false;
	}

	FMOD::Channel* channel = nullptr;
	system->playSound(it->second, nullptr, true, &channel);
	if (channel)
	{
		channel->setMode(FMOD_2D); // sounds are loaded as 3D; play this one flat
		updateVolume(volume, type);
		channel->setVolume(volume);
		channel->setPaused(false);
	}
	return true;
}
bool AudioManager::Play3D(const std::string& name, GameObject& object, AudioType type, float volume)
{
	if (!system) return false;

	const auto it = sounds.find(name);
	if (it == sounds.end())
	{
		std::cout << "[AudioManager] No sound loaded with name \"" << name << "\"\n";
		PlayEvent3D(name, object, type, volume);
		return false;
	}

	// Start paused so the sound is positioned before the first audible frame
	FMOD::Channel* channel = nullptr;
	system->playSound(it->second, nullptr, true, &channel);

	if (channel)
	{
		FMOD_VECTOR pos = Vector3ToFMOD(object.getPosition());
		FMOD_VECTOR vel = Vector3ToFMOD(object.getVelocity());
		channel->set3DAttributes(&pos, &vel);
		updateVolume(volume, type);
		channel->setVolume(volume);
		channel->setPaused(false);
	}
	return true;
}

bool AudioManager::PlayEvent(const std::string& eventPath, AudioType type, float volume) {
	if (!studioSystem) return false;

	FMOD::Studio::EventDescription* description = nullptr;
	FMOD_RESULT result = studioSystem->getEvent(eventPath.c_str(),
		&description);

	if (result != FMOD_OK) {
		std::cout << "[Audio Manager] No event \"" << eventPath << "\": " << FMOD_ErrorString(result) << "\n";
		return false;
	}

	FMOD::Studio::EventInstance* instance = nullptr;
	description->createInstance(&instance);
	updateVolume(volume, type);
	instance->setVolume(volume);
	instance->start();
	instance->release(); // Destroy when finished playing sound
	return true;
}

bool AudioManager::PlayEvent3D(const std::string& eventPath, GameObject& object, AudioType type, float volume) {
	if (!studioSystem) return false;

	FMOD::Studio::EventDescription* description = nullptr;
	FMOD_RESULT result = studioSystem->getEvent(eventPath.c_str(),
		&description);

	if (result != FMOD_OK) {
		std::cout << "[Audio Manager] No event \"" << eventPath << "\": " << FMOD_ErrorString(result) << "\n";
		return false;
	}

	// Stop the previous event on this object before starting a new one
	if (object.soundInstance != nullptr) {
		if (object.soundInstance->isValid()) {
			object.soundInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
			object.soundInstance->release(); // Release the instance to free resources
		}
		object.soundInstance = nullptr;
	}

	result = description->createInstance(&object.soundInstance);
	if (result != FMOD_OK || object.soundInstance == nullptr) {
		std::cout << "[Audio Manager] Failed to create instance for \"" << eventPath << "\": " << FMOD_ErrorString(result) << "\n";
		return false;
	}

	// Position the event before it starts so the first frame is already spatialized
	FMOD_3D_ATTRIBUTES attributes = object.get3DAttributes();
	object.soundInstance->set3DAttributes(&attributes);
	updateVolume(volume, type);
	object.soundInstance->setVolume(volume);
	object.soundInstance->start();
	object.soundInstance->release(); // Destroy when finished playing sound
	return true;
}
