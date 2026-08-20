#pragma once
#ifndef SOUND_FIELD_H
#define SOUND_FIELD_H

#include <Perception/SoundEvent.h>

#include <vector>
#include <cstddef>

struct GameMap;

/**
 * The scene's record of recent noise.
 *
 * This is the whole of the stalker's senses. The locked design is sound-only
 * perception — no vision cone, no position feed — so anything the AI learns
 * about the player has to arrive through here as a SoundEvent, and an event
 * carries where the noise *was*, never where its source is now.
 *
 * Events expire after kEventTTL so a listener cannot mine stale history: going
 * quiet has to actually work as a tactic.
 */
class SoundField
{
public:
	// Ring capacity. Oldest events are overwritten first. One room at a
	// realistic emission rate stays well under this.
	static constexpr std::size_t kCapacity = 64;

	// Seconds an event stays audible. Also the longest a stalker can keep
	// reacting to a noise the player has stopped making.
	static constexpr float kEventTTL = 5.0f;

	// Distance in world units at which loudness has fallen to half.
	static constexpr float kHalfDistance = 8.0f;

	// Multiplier applied per solid body between source and listener.
	static constexpr float kOcclusionPerBody = 0.45f;

	// Occluders counted before we stop testing. Beyond this the sound is
	// inaudible anyway and the extra raycasts are wasted.
	static constexpr int kMaxOccluders = 4;

	SoundField() { events.reserve(kCapacity); }

	// Advances the clock and drops expired events. Call once per frame.
	void Update(float deltaTime);

	// Records a noise. `timestamp` is stamped here, so callers leave it zero.
	void Emit(const SoundEvent& event);
	void Emit(Vector3 position, float loudness, SoundKind kind, std::uint64_t sourceId = 0);

	void Clear() { events.clear(); }

	float Now() const { return clock; }
	const std::vector<SoundEvent>& Events() const { return events; }

	/**
	 * How loud `event` is at `listener`, after distance falloff and occlusion.
	 * Returns 0 when inaudible. `map` may be null, which skips occlusion.
	 */
	float AudibleLoudnessAt(Vector3 listener, const SoundEvent& event, const GameMap* map) const;

	/**
	 * Strongest currently-audible event at `listener`, above `threshold`.
	 *
	 * Ties break toward the more recent event, so a listener standing between
	 * two equally loud noises follows the fresher one instead of oscillating.
	 * Events from `ignoreSourceId` are skipped — that is how the stalker avoids
	 * hearing its own footsteps and chasing itself.
	 *
	 * Returns false and leaves the out-params untouched when nothing qualifies.
	 */
	bool LoudestAudible(Vector3 listener,
	                    float threshold,
	                    std::uint64_t ignoreSourceId,
	                    const GameMap* map,
	                    SoundEvent& outEvent,
	                    float& outLoudness) const;

private:
	std::vector<SoundEvent> events = {};
	std::size_t nextSlot = 0;   // write cursor once the buffer is full
	float clock = 0.0f;
};

#endif
