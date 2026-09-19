#include <Perception/SoundField.h>

#include <gameMap.h>
#include <GameObject.h>

#include <raymath.h>
#include <algorithm>

void SoundField::Update(float deltaTime)
{
	clock += deltaTime;

	const float cutoff = clock - kEventTTL;
	const std::size_t before = events.size();
	events.erase(std::remove_if(events.begin(), events.end(),
		[cutoff](const SoundEvent& e) { return e.timestamp <= cutoff; }),
		events.end());
	if (events.size() != before)
	{
		nextSlot = events.size() < kCapacity ? events.size() : 0;
	}
}

void SoundField::Emit(const SoundEvent& event)
{
	if (event.loudness <= 0.0f) { return; }

	SoundEvent stamped = event;
	stamped.timestamp = clock;

	if (events.size() < kCapacity)
	{
		events.push_back(stamped);
		nextSlot = events.size() < kCapacity ? events.size() : 0;
		return;
	}

	// Full: overwrite the oldest slot. Losing the oldest event is correct —
	// it was closest to expiring anyway.
	events[nextSlot] = stamped;
	nextSlot = (nextSlot + 1) % kCapacity;
}

// Records a Sound Event at the position for AI perception. This class plays no
// audio itself (see the header) — a caller that wants the noise to also be
// audible plays it through AudioManager separately.
void SoundField::Emit(Vector3 position, float loudness, SoundKind kind, std::uint64_t sourceId)
{
	SoundEvent event = {};
	event.position = position;
	event.loudness = loudness;
	event.kind = kind;
	event.sourceId = sourceId;
	Emit(event);
}

float SoundField::AudibleLoudnessAt(Vector3 listener, const SoundEvent& event, const GameMap* map) const
{
	if (event.loudness <= 0.0f) { return 0.0f; }

	const float distance = Vector3Distance(listener, event.position);

	// Smooth inverse-square falloff normalised so loudness is unchanged at the
	// source and exactly halved at kHalfDistance. Plain 1/d^2 is unusable here
	// because it diverges as the stalker closes on the noise.
	const float half2 = kHalfDistance * kHalfDistance;
	float heard = event.loudness * (half2 / (half2 + distance * distance));

	if (map == nullptr || heard <= 0.0f) { return heard; }

	// Occlusion: every solid body on the straight line between source and
	// listener muffles the noise. This is what makes the room's geometry part
	// of the stealth, rather than distance alone.
	const Vector3 toListener = Vector3Subtract(listener, event.position);
	const float span = Vector3Length(toListener);
	if (span <= 0.0001f) { return heard; }

	Ray ray = {};
	ray.position = event.position;
	ray.direction = Vector3Scale(toListener, 1.0f / span);

	int occluders = 0;
	// Plain GameObjects are not the only solid geometry: minigame stations,
	// doors, and other level dressing are stored as InteractableObjects, and
	// Entities (the player, stalkers) can stand between a noise and a
	// listener too. Checking ForEachGameObject alone left every Interactable
	// and Entity unable to muffle sound. occluders is checked against the cap
	// up front (rather than only after incrementing) so it still stops
	// correctly once the three containers are visited in sequence.
	const auto considerOcclusion = [&](const GameObject& object) -> bool
	{
		if (occluders >= kMaxOccluders) { return false; }
		if (!object.isEnabled) { return true; }
		if (object.id == event.sourceId) { return true; }
		// A trigger volume is not geometry and must not muffle anything. A damage
		// zone across a corridor should not make the footsteps beyond it quieter.
		if (object.rigidBody3D.collider.isTrigger()) { return true; }

		// The collider, not the box around it, so a rotated pillar muffles sound
		// across its actual width rather than across its bounding box.
		float distance = 0.0f;
		Vector3 normal = {};
		// Only bodies strictly between the two points occlude; a body behind the
		// listener still reports a hit on an infinite ray.
		if (object.rigidBody3D.Raycast(ray, distance, normal) && distance > 0.0f && distance < span)
		{
			++occluders;
		}
		return true;
	};

	map->ForEachGameObject(considerOcclusion);
	map->ForEachEntity(considerOcclusion);
	map->ForEachInteractable(considerOcclusion);

	for (int i = 0; i < occluders; ++i) { heard *= kOcclusionPerBody; }

	return heard;
}

bool SoundField::LoudestAudible(Vector3 listener, float threshold, std::uint64_t ignoreSourceId, const GameMap* map, SoundEvent& outEvent, float& outLoudness) const
{
	bool found = false;
	float bestLoudness = threshold;
	const SoundEvent* best = nullptr;

	for (const auto& event : events)
	{
		if (event.sourceId == ignoreSourceId && ignoreSourceId != 0) { continue; }

		const float heard = AudibleLoudnessAt(listener, event, map);
		if (heard <= threshold) { continue; }

		// Strictly-greater keeps the first of equal-loudness events; the
		// timestamp tiebreak then prefers the fresher one.
		if (!found || heard > bestLoudness ||
			(heard == bestLoudness && best != nullptr && event.timestamp > best->timestamp))
		{
			bestLoudness = heard;
			best = &event;
			found = true;
		}
	}

	if (!found) { return false; }

	outEvent = *best;
	outLoudness = bestLoudness;
	return true;
}
