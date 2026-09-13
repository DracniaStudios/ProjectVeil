#pragma once
#ifndef SOUND_EVENT_H
#define SOUND_EVENT_H

#include <raylib.h>
#include <cstdint>


/// What made the noise.
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

///A single gameplay-audible noise.
struct SoundEvent
{
	Vector3       position = {};
	float         loudness = 0.0f;  // 0..1 at the source, before attenuation
	SoundKind     kind = SOUND_FOOTSTEP;
	float         timestamp = 0.0f;  // SoundField clock, seconds
	std::uint64_t sourceId = 0;      // GameObject::id, so a listener can ignore itself
};

#endif
