#pragma once
#ifndef STALKER_H
#define STALKER_H

#include <Entity.h>
#include <AI/StalkerState.h>
#include <Perception/SoundEvent.h>

#include <vector>

namespace SceneManagement { struct Scene; }
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
 * Catching is not perception and not a state. Scene::CheckCatches runs after
 * every tick's collisions, in whatever state the FSM is in, and raises a catch
 * for any player touching the stalker or within catchRadius of it. The FSM
 * itself never learns that a catch happened.
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
	// How close the stalker has to come to a player to catch them, centre to
	// centre on the ground plane (VerticalSliceDecisions tunable catchRadius).
	// Touching catches whatever this is. Checked outside the FSM.
	float catchRadius = 1.0f;
	// How much warning a player gets before contact, in seconds: the audio
	// warnings start at catchRadius + closingSpeed * this (decision 6)
	float warningLeadSeconds = 1.5f;
	// How much faster the stalker investigates a station starting than any other
	// noise (stalkerTaskSpeedMultiplier, decision 16). A parameter on Investigate,
	// not a state: it tightens the window to finish a task in one sitting.
	float taskSpeedMultiplier = 1.5f;

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

	// The investigation under way is of a station starting, so it runs at
	// taskSpeedMultiplier
	bool investigatingActivation = false;

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

	/**
	 * A search first walks to the last known position, and its timer starts only
	 * on arrival (Stalker FSM Decisions Log, decision 8). A position the stalker
	 * can't reach is given up on where its progress stops, and the sweep and the
	 * timer start from there: the nearest reachable point (decision 11).
	 */
	bool searchArrived = false;
	// Where the sweep is centred: the last known position, or the nearest point
	// reached on the way to it
	Vector3 searchAnchor = {};
	// Seconds spent sweeping since arriving. The search ends at searchDuration.
	float searchTime = 0.0f;

	// How long the stalker may make no progress toward a target before the
	// target counts as unreachable, and how much closer counts as progress
	static constexpr float kStallSeconds = 1.25f;
	static constexpr float kStallProgress = 0.25f;

	// The Director's hint, if one is waiting (read-only; see ApplyDirectorHint)
	bool HasHint() const { return hasHint; }
	Vector3 HintRegion() const { return hintRegion; }

	// Draws the patrol route and the current target in world space. Authoring a
	// route by typing coordinates is unusable; this is what makes the numbers in
	// the inspector correspond to something visible. Follows the existing
	// per-object debug display flags (displayCollider, displayDirection).
	bool displayRoute = false;

	/** Functions **/
	void update(SceneManagement::Scene* scene, float deltaTime) override;

	/**
	 * Applies a Director hint: the stalker's one sound-only hint.
	 *
	 *   What it changes: Patrol takes one detour. The stalker leaves its route,
	 *   walks to the hinted region (or as near as it can get), then rejoins the
	 *   route at the nearest waypoint. The hint is used up by that one detour.
	 *   When it fires: only in Patrol. Anything heard ends the detour and drops
	 *   the hint, and every other state ignores hints, so a hint can never
	 *   redirect an investigation, a hunt or a search.
	 *   Why it keeps the stalker non-omniscient: the region is an outstanding Task
	 *   station that nobody holds (Director, rule S-7), a world fact that says
	 *   where a player must eventually go, never where any player is. It is kept
	 *   apart from lastKnownPosition, so it is never mistaken for a noise.
	 */
	void ApplyDirectorHint(Vector3 region, float confidence);

	/**
	 * Nearest obstruction along `direction`, out to `maxDistance`.
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

	// Progress toward the current approach target, for the unreachable check
	Vector3 approachTarget = {};
	float approachBest = 0.0f;
	float approachStall = 0.0f;
	bool hasApproach = false;

	// True once the stalker has gone kStallSeconds without getting kStallProgress
	// closer to `target`. Changing the target starts the count again.
	bool ApproachStalled(Vector3 target, float deltaTime);
	void ResetApproach() { hasApproach = false; approachStall = 0.0f; }

	// Search has reached its anchor: the sweep and its timer start here
	void ArriveForSearch(Vector3 anchor);

	// The waypoint nearest the stalker, for rejoining the route after a detour
	int NearestWaypoint() const;

	/**
	 * Steer toward a target, going around world geometry in the way.
	 */
	void MoveToward(Vector3 target, float speed, float deltaTime, const GameMap* map);

	/**
	 * Pick a heading that makes progress toward `desired` without walking into
	 * a wall.
	 */
	Vector3 SteerAround(Vector3 desired, const GameMap* map) const;

	/**
	 * Distance to a target on the plane the stalker actually steers on.
	 */
	float PlanarDistanceTo(Vector3 target) const;

	// Effective threshold after the BUFF_HEARING modifier. The buff already
	// existed in Entity's enum, unused, reserved for exactly this.
	float EffectiveHearingThreshold();
	float EffectiveSearchRadius();

	void PickNextSearchTarget();
};

#endif
