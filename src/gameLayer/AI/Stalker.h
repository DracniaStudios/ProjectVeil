#pragma once
#ifndef STALKER_H
#define STALKER_H

#include <Entity.h>
#include <AI/StalkerState.h>
#include <Perception/SoundEvent.h>

#include <vector>

struct Scene;
struct GameMap;

/**
 * The slice's single enemy.
 *
 * The load-bearing invariant, and the reason this class is written the way it
 * is: the stalker never holds a pointer to the player and never reads the
 * player's position. Everything it knows arrives as a SoundEvent and is
 * flattened immediately into `lastKnownPosition` — a place a noise *was*, not
 * a thing that moves. If a future change hands this class the player, the
 * "sound only" design decision is dead no matter what the FSM still looks like.
 *
 * Physical contact is not perception and is handled elsewhere: the player
 * detects being caught in Player::onCollision, because Scene resolves
 * player-vs-entity with the player as `self`, so a collision callback on this
 * class would never fire for it.
 */
struct Stalker : Entity
{
	Stalker();

	std::unique_ptr<Entity> clone() const override { return std::make_unique<Stalker>(*this); }

	/** Tuning **/
	// Loudness that registers at all. Anything quieter is not heard.
	float hearingThreshold = 0.05f;
	// Loudness that escalates straight to a hunt rather than an investigation.
	float huntThreshold = 0.35f;
	// Seconds without a fresh stimulus before a hunt decays to a search.
	float huntPatience = 4.0f;
	// Seconds spent sweeping before giving up and returning to patrol.
	float searchDuration = 8.0f;
	// Radius of the sweep around the last known position.
	float searchRadius = 6.0f;
	// How close counts as having reached a target.
	float arriveDistance = 1.0f;

	// How far ahead the stalker looks for obstructions. Too short and it turns
	// after it is already against the wall; too long and it swerves around
	// geometry it was never going to touch.
	float avoidProbeDistance = 3.0f;

	/** Patrol route **/
	// Authored in the World Editor and serialised with the world. Empty is
	// valid: the stalker simply holds position until it hears something.
	std::vector<Vector3> waypoints = {};
	int currentWaypoint = 0;

	/** Runtime state **/
	StalkerState state = STALKER_PATROL;
	float timeInState = 0.0f;

	// Where the last heard noise came from. This is the stalker's entire model
	// of the player: a stale point, never a live position.
	Vector3 lastKnownPosition = {};
	bool    hasLastKnown = false;

	// SoundField clock reading of the last stimulus that moved the FSM.
	float lastStimulusTime = 0.0f;
	// Loudness and cause of that stimulus, kept for the inspector and for the
	// Director's decision log.
	float     lastHeardLoudness = 0.0f;
	SoundKind lastHeardKind = SOUND_FOOTSTEP;

	// Sweep target while searching, re-rolled as each one is reached.
	Vector3 searchTarget = {};

	// Draws the patrol route and the current target in world space. Authoring a
	// route by typing coordinates is unusable; this is what makes the numbers in
	// the inspector correspond to something visible. Follows the existing
	// per-object debug display flags (displayCollider, displayDirection).
	bool displayRoute = false;

	/** Functions **/
	void update(Scene* scene, float deltaTime) override;
	void render3D() override;

	// Applies a Director hint. The hint is a region worth patrolling, derived
	// from world facts only; it biases patrol and search, and is ignored
	// outright while hunting so a hint can never redirect an active pursuit.
	void ApplyDirectorHint(Vector3 region, float confidence);

	/**
	 * Nearest obstruction along `direction`, out to `maxDistance`.
	 *
	 * Returns the hit distance, or maxDistance when the path is clear. Only
	 * GameMap::gameObjects are considered: entities and interactables live in
	 * their own containers, so the stalker steers around the level but will
	 * still walk into another entity and let the collision solver separate them.
	 */
	float DistanceToObstruction(Vector3 direction, float maxDistance, const GameMap* map) const;

	/** Save Data **/
	Json formatToJson() override;
	bool loadFromJson(Json& j) override;

private:
	// Hint state, held separately from lastKnownPosition so a Director
	// suggestion can never be mistaken for something actually heard.
	Vector3 hintRegion = {};
	bool    hasHint = false;
	float   hintConfidence = 0.0f;

	void TransitionTo(StalkerState next);

	/**
	 * Steer toward a target, going around world geometry in the way.
	 *
	 * `map` may be null, which skips avoidance entirely and steers straight —
	 * that is the behaviour every caller had before avoidance existed, and it
	 * keeps a stalker driven without a world (the FSM tests) working.
	 */
	void MoveToward(Vector3 target, float speed, float deltaTime, const GameMap* map);


	/**
	 * Pick a heading that makes progress toward `desired` without walking into
	 * a wall.
	 *
	 * A whisker scan: if the straight path is clear it is used unchanged,
	 * otherwise the direction is rotated outward in steps and the first clear
	 * heading wins, falling back to whichever probe saw furthest. This is
	 * deliberately local and stateless — it handles walls and pillars, and it
	 * can still stall in a deep concave corner where every heading is blocked.
	 * A route authored with clear line of travel between waypoints never
	 * exercises that case; proper pathfinding is a Phase 2 concern once the
	 * game has more than one room.
	 */
	Vector3 SteerAround(Vector3 desired, const GameMap* map) const;

	/**
	 * Distance to a target on the plane the stalker actually steers on.
	 *
	 * MoveToward zeroes the vertical component and leaves gravity to own it, so
	 * measuring arrival in 3D asked a question the movement could never answer:
	 * a target whose height differs from the stalker's body centre by more than
	 * arriveDistance stayed permanently "not arrived", and the stalker parked on
	 * top of it forever without advancing. That is easy to hit, because
	 * waypoints are authored from the player's position and the two bodies do
	 * not share a centre height.
	 */
	float PlanarDistanceTo(Vector3 target) const;

	// Effective threshold after the BUFF_HEARING modifier. The buff already
	// existed in Entity's enum, unused, reserved for exactly this.
	float EffectiveHearingThreshold();
	float EffectiveSearchRadius();

	void PickNextSearchTarget();
};

#endif
