#pragma once
#ifndef SOUND_EVENT_H
#define SOUND_EVENT_H

#include <raylib.h>
#include <cstdint>

/**
 * What made the noise.
 *
 * The stalker treats kinds differently — a footstep is worth investigating, a
 * tampering alarm is worth hunting — so the cause travels with the event rather
 * than being inferred from loudness alone.
 */
enum SoundKind
{
	SOUND_FOOTSTEP,
	SOUND_STATION,   // a task station running
	SOUND_TAMPER,    // the player interfering with a station
	SOUND_IMPACT,    // physics collision
	SOUND_VOICE,
	SOUND_KIND_COUNT
};

inline const char* soundKindToString(int kind)
{
	switch (kind)
	{
	case SOUND_FOOTSTEP: return "Footstep";
	case SOUND_STATION:  return "Station";
	case SOUND_TAMPER:   return "Tamper";
	case SOUND_IMPACT:   return "Impact";
	case SOUND_VOICE:    return "Voice";
	default:             return "Unknown";
	}
}

/**
 * A single gameplay-audible noise.
 *
 * Deliberately separate from FMOD. AudioManager decides what the *player*
 * hears; this decides what the *AI* hears. Tying them together would make
 * perception depend on bank load state and on sounds that exist for atmosphere
 * only, and would make the stalker deaf whenever audio failed to initialise.
 */
struct SoundEvent
{
	Vector3       position = {};
	float         loudness = 0.0f;  // 0..1 at the source, before attenuation
	SoundKind     kind = SOUND_FOOTSTEP;
	float         timestamp = 0.0f;  // SoundField clock, seconds
	std::uint64_t sourceId = 0;      // GameObject::id, so a listener can ignore itself
};

#endif
