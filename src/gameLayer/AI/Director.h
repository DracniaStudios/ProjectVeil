#pragma once
#ifndef DIRECTOR_H
#define DIRECTOR_H

#include <raylib.h>

#include <vector>
#include <cstddef>

struct Scene;
struct Stalker;

/**
 * One suggestion passed to the AI, plus the world fact that produced it.
 *
 * `reason` exists because the Vertical slice's acceptance criterion is not
 * "the Director works" but "verify by logging what the Director passes". A
 * hint that cannot name the fact behind it is indistinguishable from a leak.
 */
struct DirectorHint
{
	Vector3     region = {};
	float       confidence = 0.0f;
	const char* reason = "";
	float       timestamp = 0.0f;
};

/**
 * Applies pressure without omniscience.
 *
 * The rule this class exists to enforce: the Director may read **world facts
 * only** — which stations are outstanding, how long the stalker has gone
 * without hearing anything, how long it has held a state. It must never read
 * the player's position, directly or by proxy.
 *
 * The proxy case is the subtle one and is worth stating explicitly, because
 * the obvious heuristic fails it. Hinting toward the station the player is
 * *currently running* would be a world fact by the letter of the rule and a
 * live position feed in practice: a running station is exactly where the
 * player is standing. So the heuristic below deliberately hints toward
 * stations that are **not** currently occupied — objectives the player has yet
 * to reach. That pressures the route without ever describing where they are.
 *
 * This is the pre-agreed simplified Director. The full version (hint budget,
 * tension pacing, cooldown curve) replaces the body of Evaluate() without
 * changing the call site.
 */
class Director
{
public:
	// Seconds between hints. Long enough that the stalker's own FSM, not the
	// Director, is visibly driving behaviour.
	static constexpr float kHintCooldown = 12.0f;

	// How long a stalker must have gone without hearing anything before a hint
	// is worth spending. Hinting into an active hunt would only make it worse.
	static constexpr float kQuietBeforeHint = 6.0f;

	// Retained hints. This is a debugging record, not gameplay state.
	static constexpr std::size_t kLogCapacity = 32;

	// Confidence carried by a heuristic hint. Fixed, because this version has
	// no model to be more or less sure about.
	static constexpr float kFixedConfidence = 0.5f;

	void Update(Scene* scene, float deltaTime);

	void Clear() { log.clear(); cooldown = 0.0f; clock = 0.0f; }

	const std::vector<DirectorHint>& Log() const { return log; }
	float Now() const { return clock; }
	float CooldownRemaining() const { return cooldown; }

private:
	float clock = 0.0f;
	float cooldown = 0.0f;
	std::vector<DirectorHint> log = {};

	void Record(const DirectorHint& hint);
};

#endif
