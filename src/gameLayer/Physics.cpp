#include "Physics.h"
 
#include <gameMap.h>
#include <iostream>

void RigidBody3D::checkRayCollision(const RigidBody3D& other)
{
	// Auto function to Create a Ray
	auto generateRay = [&](Vector3 position, Vector3 direciton)
		{
			Ray newRay;
			newRay.position = position;
			newRay.direction = direciton;
			return newRay;
		};
	/// Distance threshold for ray collision detection
	constexpr float touchDistance = 0.1f; // Adjust this value based on how close the ray needs to be to count as a touch

	// Auto function to check if a ray hits the other object within the touch distance
	auto hitWithinRange = [&](Ray ray)
		{
			RayCollision collision = GetRayCollisionBox(ray, other.collisionBox);
			return collision.hit && collision.distance <= Vector3Length(translation - ray.position) + touchDistance;
		};

	// Face Translation
	Vector3 c = translation;
	float hx = scale.x * 0.5f;
	float hy = scale.y * 0.5f;
	float hz = scale.z * 0.5f;
	
	// Check each face with a raycast and update touch flags accordingly
	upTouch = hitWithinRange(generateRay({ c.x, c.y + hy, c.z }, up));
	downTouch = hitWithinRange(generateRay({ c.x, c.y - hy, c.z }, down));
	frontTouch = hitWithinRange(generateRay({ c.x, c.y, c.z + hz }, forward));
	backTouch = hitWithinRange(generateRay({ c.x, c.y, c.z - hz }, back));
	rightTouch = hitWithinRange(generateRay({ c.x + hx, c.y, c.z }, right));
	leftTouch = hitWithinRange(generateRay({ c.x - hx, c.y, c.z }, left));
	/*
	upTouch = upTouch || hitWithinRange(generateRay({ c.x, c.y + hy, c.z }, up));
	downTouch = downTouch || hitWithinRange(generateRay({ c.x, c.y - hy, c.z }, down));
	frontTouch = frontTouch || hitWithinRange(generateRay({ c.x, c.y, c.z + hz }, forward));
	backTouch = backTouch || hitWithinRange(generateRay({ c.x, c.y, c.z - hz }, back));
	rightTouch = rightTouch || hitWithinRange(generateRay({ c.x + hx, c.y, c.z }, right));
	leftTouch = leftTouch || hitWithinRange(generateRay({ c.x - hx, c.y, c.z }, left));
	*/
}

bool RigidBody3D::isCollidingWith(const RigidBody3D& other) const
{
	return CheckCollisionBoxes(collisionBox, other.collisionBox);
}

Vector3 RigidBody3D::getCollisionNormal(const RigidBody3D& other) const
{
	Vector3 delta = other.getCenter() - getCenter();

	Vector3 halfA = scale * 0.5f;
	Vector3 halfB = other.scale * 0.5f;

	float overlapX = (halfA.x + halfB.x) - std::abs(delta.x);
	float overlapY = (halfA.y + halfB.y) - std::abs(delta.y);
	float overlapZ = (halfA.z + halfB.z) - std::abs(delta.z);

	// Return the normal along the axis with the smallest overlap (shallowest penetration)
	if (overlapX <= overlapY && overlapX <= overlapZ)
		return delta.x > 0 ? Vector3{ 1, 0, 0 } : Vector3{ -1, 0, 0 };
	else if (overlapY <= overlapZ)
		return delta.y > 0 ? Vector3{ 0, 1, 0 } : Vector3{ 0, -1, 0 };
	else
		return delta.z > 0 ? Vector3{ 0, 0, 1 } : Vector3{ 0, 0, -1 };
}

float RigidBody3D::getPenetrationDepth(const RigidBody3D& other) const
{
	Vector3 delta = other.getCenter() - getCenter();

	Vector3 halfA = scale * 0.5f;
	Vector3 halfB = other.scale * 0.5f;

	float overlapX = (halfA.x + halfB.x) - std::abs(delta.x);
	float overlapY = (halfA.y + halfB.y) - std::abs(delta.y);
	float overlapZ = (halfA.z + halfB.z) - std::abs(delta.z);

	return std::min({ overlapX, overlapY, overlapZ });
}

// ─── Constraint Resolution ─────────────────────────────────────────────────

void RigidBody3D::resolveConstrains(RigidBody3D* other)
{
	if (other == this) return;
		
	// Reset Position to prevent tunneling
	resolveCollision(*other);
	
	// Update Collision Flags
	checkRayCollision(*other);
	
	if (isCollidingWith(*other))
	{

		// Call Collision Event on Owner
		//owner->onHit(other->owner);

		// Set collidingWith to other object's owner
		if (other->owner)
		{
			collidingWith = other->owner;
		}

		isColliding = true;
	}
	else
	{
		collidingWith = nullptr;
		isColliding = false;
	}
}

void RigidBody3D::resolveConstrains(RigidBody3D* otherObjects, int objectCount)
{
	if (otherObjects == nullptr || objectCount <= 0)return;

	for (int i = 0; i < objectCount; i++)
	{
		if (&otherObjects[i] == this) continue;

		if (isCollidingWith(otherObjects[i]))
		{
			// Reset Position to prevent tunneling
			resolveCollision(otherObjects[i]);
			
			// Update Collision Flags 
			checkRayCollision(otherObjects[i]);
			
			// Call Collision Event on Owner
			owner->onHit(otherObjects[i].owner);
			
			if (otherObjects[i].owner)
			{
				collidingWith = otherObjects[i].owner;
			}

			isColliding = true;
		}
		else
		{
			collidingWith = nullptr;
			isColliding = false;
		}
	}
}

// ─── Collision Response ────────────────────────────────────────────────────

void RigidBody3D::resolveCollision(RigidBody3D& other)
{
	if (!isCollidingWith(other)) return;
	if (isStatic && other.isStatic) return;

	// If this is static, let the dynamic body handle it to keep logic in one place
	if (isStatic && !other.isStatic)
	{
		other.resolveCollision(*this);
		return;
	}

	Vector3 normal = getCollisionNormal(other);
	float penetration = getPenetrationDepth(other);

	// Positional Correction
	const float slop = 0.01f;// Ignore small penetrations to prevent jitter
	const float baumgarte = 0.3f;// Correct a fraction of penetrations per frame

	float correction = std::max(penetration - slop, 0.0f) * baumgarte;

	// Small bias prevents objects resting flush from re-triggering next frame
	//const float bias = 0.001f;
	//float separation = penetration + bias;

	// ── Positional correction ──────────────────────────────────────────────
	if (!isStatic && !other.isStatic)
	{
		// Both dynamic: push each half as far
		translation -= normal * (correction * 0.5f);
		other.translation += normal * (correction * 0.5f);
	}
	else if (!isStatic)
	{
		// Only this is dynamic: absorb the full separation
		translation -= normal * correction;
	}

	// ── Velocity response ──────────────────────────────────────────────────
	Vector3 relativeVelocity = velocity - other.velocity;
	float velocityAlongNormal = Vector3DotProduct(relativeVelocity, normal);

	// Only respond if objects are still moving toward each other
	if (velocityAlongNormal >= 0) return;

	// Clamp small closing velocities to prevent jitter
	const float restingThreshold = 0.1f;
	if (std::abs(velocityAlongNormal) < restingThreshold)
	{
		// Fixes jittering across the Y Velocity
		if (!isStatic && !other.isStatic)
		{

			float avgX = (velocity.x + other.velocity.x) * 0.5f;
			velocity.x = avgX;
			other.velocity.x = avgX;

			float avgY = (velocity.y + other.velocity.y) * 0.5f;
			velocity.y = avgY;
			other.velocity.y = avgY;

			float avgZ = (velocity.z + other.velocity.z) * 0.5f;
			velocity.z = avgZ;
			other.velocity.z = avgZ;

		}
		else if (!isStatic)
		{
			velocity.x = 0;
			velocity.y = 0;
			velocity.z = 0;
		}
	}

	const float restitution = 0.0f; // 0 = no bounce, 1 = perfectly elastic
	float impulse = -(1.0f + restitution) * velocityAlongNormal;

	if (!isStatic && !other.isStatic)
	{
		float halfImpulse = impulse * 0.5f;
		velocity += normal * halfImpulse;
		other.velocity -= normal * halfImpulse;
	}
	else if (!isStatic)
	{
		velocity += normal * impulse;
	}

	// ── Friction (tangential damping) ──────────────────────────────────────
	Vector3 tangent = relativeVelocity - (normal * Vector3DotProduct(relativeVelocity, normal));
	float tangentLen = Vector3Length(tangent);

	if (tangentLen > 0.001f)
	{
		Vector3 tangentDir = Vector3Normalize(tangent);
		const float friction = 0.1f;
		float frictionImpulse = tangentLen * friction;

		if (!isStatic && !other.isStatic)
		{
			velocity -= tangentDir * (frictionImpulse * 0.5f);
			other.velocity += tangentDir * (frictionImpulse * 0.5f);
		}
		else if (!isStatic)
		{
			velocity -= tangentDir * frictionImpulse;
		}
	}

}

void RigidBody3D::updateForce(GameMap gameMap, float deltaTime)
{
	// Control Air Time
	if (downTouch){ airTime = 0;}
	else { airTime += deltaTime; }

	if (airTime > 0)
	{
		velocity.y += deltaTime;
	}

	// Apply acceleration to velocity
	velocity += acceleration * deltaTime;

	// Limit velocity to max speed
	if (Vector3Length(velocity) > 999)
	{
		velocity = Vector3Scale(Vector3Normalize(velocity), 999);
	}
	
	// Apply velocity to position
	translation += velocity * deltaTime;
	
	// Apply drag to velocity
	velocity *= drag;
	
	// Reset acceleration for the next frame
	acceleration = {};

	// Apply Touch Detection
	
	// Ground Detection
	/*
	if (translation.y < 0.0f + scale.y / 2)
	{
		translation.y = 0.0f + scale.y / 2;
		velocity.y = 0.0f;
		downTouch = true;
	}
	else
	{
		downTouch = false;
		airTime += deltaTime;
	}
	*/

	for (auto& obj : gameMap.gameObjects)
	{
		checkRayCollision(*obj->rigidBody3D);
	}

}

void RigidBody3D::update(GameMap gameMap,float deltaTime)
{
	if (scale == Vector3Zero()) { scale = Vector3One(); }
	if (!isEnabled) return;
	if (!isStatic) {
		if (useGravity) applyGravity();
		updateForce(gameMap, deltaTime);
	}
	else
	{
		velocity = {};
		acceleration = {};
	}
	lastPosition = translation;

	// Update collision box to match current position and scale
	collisionBox.min = { translation.x - scale.x * 0.5f, translation.y - scale.y * 0.5f, translation.z - scale.z * 0.5f };
	collisionBox.max = { translation.x + scale.x * 0.5f, translation.y + scale.y * 0.5f, translation.z + scale.z * 0.5f };
}

/// ------------------- RigidBody2D Collision Detection and Resolution ------------------- ///

void RigidBody2D::resolveConstrains(GameMap& mapData)
{

}

void RigidBody2D::checkCollisionOnce(Vector2& position, GameMap& mapData)
{
	
}

Vector2 RigidBody2D::performCollisionOnOneAxis(GameMap& mapData, Vector2 position, Vector2 delta)
{
	return Vector2Zero();
}

void RigidBody2D::updateForce(float deltaTime)
{
	velocity += acceleration * deltaTime;
	if (Vector2Length(velocity) > maxSpeed)
	{
		velocity = Vector2Scale(Vector2Normalize(velocity), maxSpeed);
	}
	translation.x += velocity.x * deltaTime;
	translation.y += velocity.y * deltaTime;

	// Universal drag ( air resisitance, friction, etc. )
	Vector2 dragVector = Vector2{ velocity.x * std::abs(velocity.x), velocity.y * std::abs(velocity.y) };
	float drag = 0.01f; // Adjust this value to increase/decrease drag strength

	if (Vector2Length(dragVector) * drag * deltaTime > Vector2Length(velocity))
	{
		velocity = {};
	}
	else
	{
		velocity -= dragVector * drag * deltaTime;
	}

	if (Vector2Length(velocity) <= 0.01f)
	{
		velocity = {};
	}

	acceleration = {};

	// Ground Rules
	
	if (translation.y > GetScreenHeight() - scale.y) { translation.y = GetScreenHeight() - scale.y; }
	if (translation.y < scale.y) { translation.y = scale.y; }
	if (translation.x > GetScreenWidth() - scale.x) { translation.x = GetScreenWidth() - scale.x; }
	if (translation.x < scale.x) { translation.x = scale.x; }
	

}
void RigidBody2D::update(float deltaTime)
{
	if (scale == Vector3Zero()) { scale = Vector3One(); }

	if (!isEnabled) return;
	if (isStatic) return;

	if (useGravity) applyGravity();

	updateForce(deltaTime);
	
	lastPosition = Vector2(translation.x, translation.y);
	
}
