#include <Perception/SoundField.h>

#include <gameMap.h>
#include <GameObject.h>

#include <raymath.h>
#include <algorithm>

void SoundField::Update(float deltaTime)
{
	clock += deltaTime;

	// Expiry is a compaction rather than an erase-per-element: events are
	// emitted in clock order, so the survivors keep their ordering and the
	// write cursor can simply resume at the end.
	const float cutoff = clock - kEventTTL;
	if (!events.empty() && events.front().timestamp <= cutoff)
	{
		events.erase(std::remove_if(events.begin(), events.end(),
			[cutoff](const SoundEvent& e) { return e.timestamp <= cutoff; }),
			events.end());
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
	for (const auto& object : map->gameObjects)
	{
		if (!object.isEnabled) { continue; }
		if (object.id == event.sourceId) { continue; }

		const RayCollision hit = GetRayCollisionBox(ray, object.rigidBody3D.collisionBox);
		// Only bodies strictly between the two points occlude; a box behind the
		// listener still reports a hit on an infinite ray.
		if (hit.hit && hit.distance > 0.0f && hit.distance < span)
		{
			if (++occluders >= kMaxOccluders) { break; }
		}
	}

	for (int i = 0; i < occluders; ++i) { heard *= kOcclusionPerBody; }

	return heard;
}

bool SoundField::LoudestAudible(Vector3 listener,
                                float threshold,
                                std::uint64_t ignoreSourceId,
                                const GameMap* map,
                                SoundEvent& outEvent,
                                float& outLoudness) const
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
