#pragma once
#ifndef CLIENT_STALKER_WARNINGS_H
#define CLIENT_STALKER_WARNINGS_H

// The stalker's audio warnings (MultiplayerSplitPlan.md G4; VerticalSliceDecisions
// decisions 4 to 7). A catch in any state ends the round, so the player must
// hear the stalker coming first:
//
//   moving      its footsteps, at catchRadius + its current speed * lead
//   standing    its breathing, at catchRadius + the player's walk speed * lead
//
// Played by each client from what it draws (the server's scene on a Host, the
// Snapshot-driven copy on a joined client), so no SoundCue crosses the wire.
// None of it enters the SoundField: the stalker never hears its own warnings.

#include <raylib.h>

namespace SceneManagement { struct Scene; }

namespace Client {

	enum class StalkerWarning { None, Footsteps, Breathing };

	// decision 6: catchRadius + closingSpeed * warningLeadSeconds
	inline float WarningRadius(float catchRadius, float closingSpeed, float leadSeconds)
	{
		return catchRadius + closingSpeed * leadSeconds;
	}

	// The speed under which the stalker counts as standing still, units/s
	inline constexpr float kStalkerStillSpeed = 0.3f;

	/**
	 * Which warning a player at `distance` from the stalker hears. Footsteps use
	 * the stalker's current speed, so a boosted stalker is heard from farther
	 * away; breathing uses the player's walk speed, so a sprinting player gets
	 * less warning, by design.
	 */
	StalkerWarning WarningFor(float distance, float stalkerSpeed, float playerWalkSpeed, float catchRadius, float leadSeconds);

	// Per frame: plays the warning this client's player should hear
	void UpdateStalkerWarnings(SceneManagement::Scene& scene, float frameTime);

	// A station starting, heard by everyone (played when a claim is Granted)
	void PlayStationActivation(Vector3 position);

	// The sound names, with the placeholders played until real files exist in
	// resources/audio
	inline constexpr const char* kStalkerFootstepSound = "Stalker_Footstep";
	inline constexpr const char* kStalkerFootstepFallback = "Jump_Landing_With_Small_Coins";
	inline constexpr const char* kStalkerBreathingSound = "Stalker_Breathing";
	inline constexpr const char* kStalkerBreathingFallback = "test_beep";
	inline constexpr const char* kStationActivationSound = "Station_Activation";
	inline constexpr const char* kStationActivationFallback = "station_intro";
}

#endif
