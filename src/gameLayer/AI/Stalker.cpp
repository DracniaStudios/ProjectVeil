#include <AI/Stalker.h>

#include <Scene.h>
#include <SceneManager.h>
#include <Perception/SoundField.h>

#include <raymath.h>
#include <cmath>

Stalker::Stalker()
{
	kind = ENTITYKIND_STALKER;
	type = OBJECT_ENTITY;
	name = "Stalker";

	maxHealth = 100.0f;
	baseSpeed = 2.0f;
	baseDamage = 25.0f;
}

float Stalker::EffectiveHearingThreshold()
{
	// BUFF_HEARING sharpens the ears: a lower threshold means quieter noises
	// register. Halving is deliberate — doubling sensitivity is a noticeable
	// difficulty swing without making crouch-walking pointless.
	if (auto* buff = getBuff(BUFF_HEARING); buff && buff->remaining_time() > 0)
	{
		return hearingThreshold * 0.5f;
	}
	return hearingThreshold;
}

float Stalker::EffectiveSearchRadius()
{
	if (auto* buff = getBuff(BUFF_SEARCH); buff && buff->remaining_time() > 0)
	{
		return searchRadius * 1.5f;
	}
	return searchRadius;
}

void Stalker::TransitionTo(StalkerState next)
{
	if (next == state) { return; }
	state = next;
	timeInState = 0.0f;

	if (next == STALKER_SEARCH_LAST_KNOWN) { PickNextSearchTarget(); }
}

void Stalker::PickNextSearchTarget()
{
	// Sweep points are drawn around the last known position rather than around
	// the stalker, so the search stays anchored to where the noise was even as
	// the stalker wanders across the room.
	const float radius = EffectiveSearchRadius();

	// Deterministic-ish spiral rather than an RNG draw: the search reads as
	// purposeful sweeping instead of twitching, and it is reproducible when
	// debugging a transition.
	const float angle = timeInState * 2.4f + static_cast<float>(currentWaypoint);
	searchTarget = Vector3{
		lastKnownPosition.x + std::cos(angle) * radius,
		lastKnownPosition.y,
		lastKnownPosition.z + std::sin(angle) * radius
	};
}

float Stalker::PlanarDistanceTo(Vector3 target) const
{
	const Vector3 self = getPosition();
	return Vector2Distance(Vector2{ self.x, self.z }, Vector2{ target.x, target.z });
}

// Rotation about the vertical axis. Steering is planar, so this is the only
// rotation the avoidance needs.
static Vector3 RotateAroundY(Vector3 v, float radians)
{
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	return Vector3{ v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

float Stalker::DistanceToObstruction(Vector3 direction, float maxDistance, const GameMap* map) const
{
	if (map == nullptr) { return maxDistance; }

	Ray ray = {};
	// Cast from the body's centre rather than its base: a ray along the floor
	// grazes every surface it slides over and reports a permanent obstruction.
	ray.position = getPosition();
	ray.direction = direction;

	float nearest = maxDistance;

	for (const auto& object : map->gameObjects)
	{
		if (!object.isEnabled) { continue; }
		// Anything the solver will not stop the stalker against should not stop
		// it here either, or the AI flinches away from things it can walk through.
		if (!object.rigidBody3D.canCollide) { continue; }

		const RayCollision hit = GetRayCollisionBox(ray, object.rigidBody3D.collisionBox);
		if (hit.hit && hit.distance >= 0.0f && hit.distance < nearest)
		{
			nearest = hit.distance;
		}
	}

	return nearest;
}

Vector3 Stalker::SteerAround(Vector3 desired, const GameMap* map) const
{
	if (map == nullptr) { return desired; }

	const float probe = avoidProbeDistance;
	if (DistanceToObstruction(desired, probe, map) >= probe) { return desired; }

	// Blocked ahead. Fan outward in widening pairs and take the first heading
	// that is clear, trying both sides at each angle so the stalker prefers the
	// smallest deviation that works rather than always turning one way.
	constexpr float kStepDegrees = 20.0f;
	constexpr int kSteps = 6; // out to 120 degrees either side
	constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

	Vector3 bestDirection = desired;
	float bestClearance = DistanceToObstruction(desired, probe, map);

	for (int step = 1; step <= kSteps; ++step)
	{
		const float angle = static_cast<float>(step) * kStepDegrees * kDegToRad;

		for (const float sign : { 1.0f, -1.0f })
		{
			const Vector3 candidate = RotateAroundY(desired, angle * sign);
			const float clearance = DistanceToObstruction(candidate, probe, map);

			if (clearance >= probe) { return candidate; }

			if (clearance > bestClearance)
			{
				bestClearance = clearance;
				bestDirection = candidate;
			}
		}
	}

	// Everything is blocked — a concave corner. Take the roomiest heading and
	// keep moving rather than freezing; the collision solver handles the rest.
	return bestDirection;
}

void Stalker::MoveToward(Vector3 target, float speed, float deltaTime, const GameMap* map)
{
	Vector3 delta = Vector3Subtract(target, getPosition());
	delta.y = 0.0f; // steering is planar; gravity owns the vertical axis

	const float distance = Vector3Length(delta);
	if (distance < 0.0001f) { return; }

	Vector3 direction = Vector3Scale(delta, 1.0f / distance);

	// Only steer around what is actually between here and the target. Probing
	// the full distance would make the stalker swerve around a wall it intends
	// to stop short of.
	if (distance > arriveDistance)
	{
		direction = SteerAround(direction, map);
	}

	// Preserve the vertical component so the body still falls and lands
	// normally — overwriting it would leave the stalker hovering.
	const Vector3 velocity = rigidBody3D.GetVelocity();
	rigidBody3D.SetVelocity(direction.x * speed, velocity.y, direction.z * speed);

	// Face the direction of travel so the debug direction gizmo and any future
	// model orientation follow the AI rather than lagging it.
	rigidBody3D.forward = direction;
}

void Stalker::ApplyDirectorHint(Vector3 region, float confidence)
{
	// A hint never overrides a live pursuit. The Director works from world
	// facts and has no idea where the player is, so letting it steer a hunt
	// would make the stalker worse, not better.
	if (state == STALKER_HUNT) { return; }

	hintRegion = region;
	hintConfidence = confidence;
	hasHint = true;
}

void Stalker::render3D()
{
	Entity::render3D();

	if (!displayRoute || !isEnabled) { return; }

	// Patrol route: a node per waypoint and the loop that joins them, drawn
	// closed because PATROL wraps back to index 0 rather than reversing.
	for (std::size_t i = 0; i < waypoints.size(); ++i)
	{
		const bool isCurrent = (static_cast<int>(i) == currentWaypoint);
		DrawSphere(waypoints[i], isCurrent ? 0.45f : 0.3f, isCurrent ? YELLOW : SKYBLUE);
		DrawLine3D(waypoints[i], waypoints[(i + 1) % waypoints.size()], SKYBLUE);
	}

	// Where the stalker is actually heading right now, which is only the patrol
	// route while it is patrolling — the other three states steer off belief.
	Vector3 target = {};
	bool hasTarget = false;
	switch (state)
	{
	case STALKER_PATROL:
		if (!waypoints.empty() && currentWaypoint < static_cast<int>(waypoints.size()))
		{
			target = waypoints[currentWaypoint];
			hasTarget = true;
		}
		break;
	case STALKER_INVESTIGATE:
	case STALKER_HUNT:
		target = lastKnownPosition;
		hasTarget = hasLastKnown;
		break;
	case STALKER_SEARCH_LAST_KNOWN:
		target = searchTarget;
		hasTarget = true;
		break;
	default:
		break;
	}

	if (hasTarget)
	{
		// Red for a live pursuit, orange otherwise: the colour says which of the
		// two kinds of target this is without reading the state text.
		const Color colour = (state == STALKER_HUNT) ? RED : ORANGE;
		DrawLine3D(getPosition(), target, colour);
		DrawSphere(target, 0.25f, colour);
	}

	// The belief itself, drawn even when it is not the current target, so a
	// stale last-known position stays visible while the stalker sweeps away
	// from it.
	if (hasLastKnown) { DrawSphereWires(lastKnownPosition, 0.6f, 6, 6, MAROON); }
}

void Stalker::update(Scene* scene, float deltaTime)
{
	// Entity::update owns the stamina economy and recomputes currentSpeed from
	// baseSpeed plus the sprint/crouch flags, so gait changes below take effect
	// through the same path the player uses.
	Entity::update(scene, deltaTime);

	if (scene == nullptr || !isEnabled) { return; }

	timeInState += deltaTime;

	// --- Perception -------------------------------------------------------
	// The single input. Note the ignore-id: without it the stalker hears its
	// own emissions and chases itself around the room.
	SoundEvent heard = {};
	float heardLoudness = 0.0f;
	const bool didHear = scene->soundField.LoudestAudible(
		getPosition(), EffectiveHearingThreshold(), id, &scene->gameMap, heard, heardLoudness);

	if (didHear)
	{
		// Flattened immediately to a position. Nothing downstream can ask a
		// follow-up question about the source.
		lastKnownPosition = heard.position;
		hasLastKnown = true;
		lastStimulusTime = scene->soundField.Now();
		lastHeardLoudness = heardLoudness;
		lastHeardKind = heard.kind;
	}

	const float stimulusAge = scene->soundField.Now() - lastStimulusTime;

	// --- States -----------------------------------------------------------
	switch (state)
	{
	case STALKER_PATROL:
	{
		isSprinting = false;
		isCrouching = false;

		if (didHear)
		{
			TransitionTo(heardLoudness >= huntThreshold ? STALKER_HUNT : STALKER_INVESTIGATE);
			break;
		}

		// A Director hint biases where patrolling happens, but only when there
		// is no real route authored or the hint is confident enough to be worth
		// a detour. It is a suggestion about the room, not about the player.
		if (waypoints.empty())
		{
			if (hasHint) { MoveToward(hintRegion, currentSpeed, deltaTime, &scene->gameMap); }
			break;
		}

		currentWaypoint %= static_cast<int>(waypoints.size());
		const Vector3 target = waypoints[currentWaypoint];
		MoveToward(target, currentSpeed, deltaTime, &scene->gameMap);

		if (PlanarDistanceTo(target) <= arriveDistance)
		{
			currentWaypoint = (currentWaypoint + 1) % static_cast<int>(waypoints.size());
		}
		break;
	}

	case STALKER_INVESTIGATE:
	{
		isSprinting = false;
		isCrouching = false;

		// A louder noise mid-investigation escalates rather than re-targets.
		if (didHear && heardLoudness >= huntThreshold)
		{
			TransitionTo(STALKER_HUNT);
			break;
		}

		if (!hasLastKnown) { TransitionTo(STALKER_PATROL); break; }

		// Investigation moves at base pace — the stalker is curious, not
		// alerted, and the slower approach is what gives the player room to
		// reposition after a single noise.
		MoveToward(lastKnownPosition, currentSpeed * 0.75f, deltaTime, &scene->gameMap);

		if (PlanarDistanceTo(lastKnownPosition) <= arriveDistance)
		{
			// Arrived and found nothing: sweep the area before giving up.
			TransitionTo(STALKER_SEARCH_LAST_KNOWN);
		}
		break;
	}

	case STALKER_HUNT:
	{
		// Sprinting routes through Entity::update's speed model, so the hunt
		// speed is the same multiplier the player gets and stays consistent if
		// that tuning changes.
		isSprinting = true;
		isCrouching = false;

		if (!hasLastKnown) { TransitionTo(STALKER_PATROL); break; }

		MoveToward(lastKnownPosition, currentSpeed, deltaTime, &scene->gameMap);

		// Losing the trail is a timeout on *stimulus*, not on arrival: standing
		// on the last known position while the player keeps making noise nearby
		// should keep the hunt alive.
		if (stimulusAge >= huntPatience)
		{
			TransitionTo(STALKER_SEARCH_LAST_KNOWN);
		}
		break;
	}

	case STALKER_SEARCH_LAST_KNOWN:
	{
		isSprinting = false;
		isCrouching = false;

		if (didHear)
		{
			TransitionTo(heardLoudness >= huntThreshold ? STALKER_HUNT : STALKER_INVESTIGATE);
			break;
		}

		MoveToward(searchTarget, currentSpeed, deltaTime, &scene->gameMap);

		if (PlanarDistanceTo(searchTarget) <= arriveDistance)
		{
			PickNextSearchTarget();
		}

		if (timeInState >= searchDuration)
		{
			// Give up. The stale position is cleared so a later state cannot
			// act on a memory the stalker has already abandoned.
			hasLastKnown = false;
			TransitionTo(STALKER_PATROL);
		}
		break;
	}

	default:
		TransitionTo(STALKER_PATROL);
		break;
	}
}

/** Save Data **/

Json Stalker::formatToJson()
{
	Json j = Entity::formatToJson();

	j["State"] = static_cast<int>(state);
	j["HearingThreshold"] = hearingThreshold;
	j["HuntThreshold"] = huntThreshold;
	j["HuntPatience"] = huntPatience;
	j["SearchDuration"] = searchDuration;
	j["SearchRadius"] = searchRadius;
	j["ArriveDistance"] = arriveDistance;
	j["AvoidProbeDistance"] = avoidProbeDistance;
	j["CurrentWaypoint"] = currentWaypoint;

	Json route = Json::array();
	for (const auto& point : waypoints)
	{
		route.push_back(Json{ { "x", point.x }, { "y", point.y }, { "z", point.z } });
	}
	j["Waypoints"] = route;

	// lastKnownPosition is deliberately not persisted. It is a live belief
	// about a noise that has long expired by the time a save is reloaded;
	// restoring it would have the stalker resume hunting a position the player
	// left in a previous session.

	return j;
}

bool Stalker::loadFromJson(Json& j)
{
	if (!Entity::loadFromJson(j)) { return false; }

	state = static_cast<StalkerState>(j.value("State", static_cast<int>(STALKER_PATROL)));
	if (state < 0 || state >= STALKER_STATE_COUNT) { state = STALKER_PATROL; }

	hearingThreshold = j.value("HearingThreshold", hearingThreshold);
	huntThreshold = j.value("HuntThreshold", huntThreshold);
	huntPatience = j.value("HuntPatience", huntPatience);
	searchDuration = j.value("SearchDuration", searchDuration);
	searchRadius = j.value("SearchRadius", searchRadius);
	arriveDistance = j.value("ArriveDistance", arriveDistance);
	avoidProbeDistance = j.value("AvoidProbeDistance", avoidProbeDistance);
	currentWaypoint = j.value("CurrentWaypoint", currentWaypoint);

	waypoints.clear();
	if (j.contains("Waypoints") && j["Waypoints"].is_array())
	{
		for (auto& point : j["Waypoints"])
		{
			waypoints.push_back(Vector3{
				point.value("x", 0.0f), point.value("y", 0.0f), point.value("z", 0.0f) });
		}
	}

	// A loaded stalker starts with no belief and no hint, whatever it was
	// doing when the save was written.
	hasLastKnown = false;
	timeInState = 0.0f;
	lastStimulusTime = 0.0f;

	// STALKER_INVESTIGATE and STALKER_HUNT self-correct on the next update()
	// because both check hasLastKnown before doing anything else. Search has
	// no such guard — it steers straight at `searchTarget`, which is not
	// persisted and would otherwise reload as the default (0,0,0), sending a
	// stalker loaded mid-search sprinting toward the world origin for up to
	// searchDuration seconds. Nothing about "searching" survives a save, so
	// land it on patrol like the other belief-dependent states effectively do.
	if (state == STALKER_SEARCH_LAST_KNOWN) { state = STALKER_PATROL; }

	return true;
}
