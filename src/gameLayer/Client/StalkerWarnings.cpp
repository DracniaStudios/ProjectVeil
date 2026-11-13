#include "StalkerWarnings.h"

#include <Scene.h>
#include <AI/Stalker.h>
#include <AudioManager.h>
#include <Client/LocalPlayer.h>

#include <raymath.h>

namespace Client {

	namespace {
		// The interval between footfalls, walking and running, and between breaths
		constexpr float kWalkStepInterval = 0.5f;
		constexpr float kRunStepInterval = 0.3f;
		constexpr float kBreathInterval = 2.2f;

		struct WarningState
		{
			const SceneManagement::Scene* scene = nullptr;
			std::uint64_t stalkerId = 0;
			Vector3 lastPosition = {};
			bool havePosition = false;
			float speed = 0.0f;
			float stepTimer = 0.0f;
			float breathTimer = 0.0f;
		};

		WarningState& State()
		{
			static WarningState state;
			return state;
		}

		void PlayNamed(const char* name, const char* fallback, Vector3 at, float volume)
		{
			auto& audio = AudioManager::AudioManager::getInstance();
			audio.Play3D(audio.HasSound(name) ? name : fallback, at, AudioManager::AUDIO_GAMEPLAY_SFX, volume);
		}

		Stalker* FindStalker(SceneManagement::Scene& scene)
		{
			Stalker* found = nullptr;
			scene.gameMap.ForEachStalker([&](Stalker& s) {
				if (!s.isEnabled) { return true; }
				found = &s;
				return false;
			});
			return found;
		}
	}

	StalkerWarning WarningFor(float distance, float stalkerSpeed, float playerWalkSpeed, float catchRadius, float leadSeconds)
	{
		if (stalkerSpeed >= kStalkerStillSpeed)
		{
			return distance <= WarningRadius(catchRadius, stalkerSpeed, leadSeconds) ? StalkerWarning::Footsteps : StalkerWarning::None;
		}
		return distance <= WarningRadius(catchRadius, playerWalkSpeed, leadSeconds) ? StalkerWarning::Breathing : StalkerWarning::None;
	}

	void UpdateStalkerWarnings(SceneManagement::Scene& scene, float frameTime)
	{
		WarningState& state = State();
		Stalker* stalker = FindStalker(scene);
		Player* player = LocalPlayer(scene);

		if (stalker == nullptr || player == nullptr || frameTime <= 0.0f || scene.overlays.IsOpen(Overlay::Pause))
		{
			state.havePosition = false;
			return;
		}
		if (state.scene != &scene || state.stalkerId != stalker->id)
		{
			state = WarningState{};
			state.scene = &scene;
			state.stalkerId = stalker->id;
		}

		// The stalker's speed as this client sees it move, which works the same
		// for the server's own scene and for a Snapshot-driven copy
		const Vector3 at = stalker->getPosition();
		if (state.havePosition)
		{
			const float moved = Vector2Distance(Vector2{ at.x, at.z }, Vector2{ state.lastPosition.x, state.lastPosition.z });
			const float instant = moved / frameTime;
			state.speed += (instant - state.speed) * std::min(1.0f, frameTime * 8.0f); // smoothed over a few frames
		}
		state.lastPosition = at;
		state.havePosition = true;

		// The player's walking speed: what Player::update3D moves at, per second
		const float playerWalkSpeed = player->baseSpeed * 0.1f * 60.0f;
		const Vector3 me = player->getPosition();
		const float distance = Vector2Distance(Vector2{ at.x, at.z }, Vector2{ me.x, me.z });

		switch (WarningFor(distance, state.speed, playerWalkSpeed, stalker->catchRadius, stalker->warningLeadSeconds))
		{
		case StalkerWarning::Footsteps:
			state.breathTimer = 0.0f;
			state.stepTimer -= frameTime;
			if (state.stepTimer <= 0.0f)
			{
				state.stepTimer = stalker->isSprinting ? kRunStepInterval : kWalkStepInterval;
				PlayNamed(kStalkerFootstepSound, kStalkerFootstepFallback, at, stalker->isSprinting ? 1.0f : 0.7f);
			}
			break;
		case StalkerWarning::Breathing:
			state.stepTimer = 0.0f;
			state.breathTimer -= frameTime;
			if (state.breathTimer <= 0.0f)
			{
				state.breathTimer = kBreathInterval;
				PlayNamed(kStalkerBreathingSound, kStalkerBreathingFallback, at, 0.6f);
			}
			break;
		case StalkerWarning::None:
			state.stepTimer = 0.0f;
			state.breathTimer = 0.0f;
			break;
		}
	}

	void PlayStationActivation(Vector3 position)
	{
		PlayNamed(kStationActivationSound, kStationActivationFallback, position, 1.0f);
	}
}
