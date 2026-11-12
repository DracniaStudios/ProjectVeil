#pragma once
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <fmod_studio.hpp>
#include <fmod.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <raylib.h>

struct GameObject;
struct Player;
namespace SceneManagement { struct Scene; }

namespace AudioManager {

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

	// A GameObject's 3D attributes, for the sound it carries
	FMOD_3D_ATTRIBUTES AttributesOf(const GameObject& object);

	// The listener for a player; listenerPosition is where the camera is
	FMOD_3D_ATTRIBUTES ListenerFor(const Player& player, Vector3 listenerPosition);

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
		// Sets each kind of sound's volume from Settings. Sounds already playing
		// from files change with it; FMOD events take it when they next start.
		void ApplySettings();
		// Hears the world from the scene's player (nothing before the first scene
		// is built), then lets FMOD run
		void update(SceneManagement::Scene* scene);
		void shutdown();

		// File Audio
		bool Play(const std::string& name, AudioType type = AUDIO_NONE, float volume = 1.0f);
		bool Play3D(const std::string& name, GameObject& object, AudioType type = AUDIO_NONE, float volume = 1.0f);
		bool Play3D(const std::string& name, Vector3 position, AudioType type = AUDIO_NONE, float volume = 1.0f);

		// Whether a sound file with this name was loaded (no event lookup)
		bool HasSound(const std::string& name) const { return sounds.find(name) != sounds.end(); }

		// Bank Audio
		bool PlayEvent(const std::string& eventPath, AudioType type = AUDIO_NONE, float volume = 1.0f);
		bool PlayEvent3D(const std::string& eventPath, GameObject& object, AudioType type = AUDIO_NONE, float volume = 1.0f);
		bool PlayEvent3D(const std::string& eventPath, Vector3 position, AudioType type = AUDIO_NONE, float volume = 1.0f);

		// End Audio
		void StopAudio();

	private:
		// The group a file sound of that kind plays in, or nullptr when FMOD couldn't make one
		FMOD::ChannelGroup* GroupFor(AudioType type) const;
		// What a file sound's own channel is set to: the group carries the
		// Settings volume, so only a sound without one carries it itself
		float ChannelVolume(AudioType type, float volume) const;

		FMOD::System* system = nullptr;
		std::array<FMOD::ChannelGroup*, AUDIO_DIALOGUE + 1> channelGroups = {}; // By AudioType
		std::unordered_map<std::string, FMOD::Sound*> sounds;
		FMOD::Studio::System* studioSystem = nullptr;
		std::vector<FMOD::Studio::Bank*> banks;
	};
}
#endif
