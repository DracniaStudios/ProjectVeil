#include "Physics.h"
 
#include <gameMap.h>
#include <iostream>

void RigidBody3D::checkRayCollision(const RigidBody3D& other)
{
	auto generateRay = [&](Vector3 position, Vector3 direciton)
		{
			Ray newRay;
			newRay.position = position;
			newRay.direction = direciton;
			return newRay;
		};

	upTouch = GetRayCollisionBox(generateRay(translation, up), other.collisionBox).hit;
	downTouch = GetRayCollisionBox(generateRay(translation, down), other.collisionBox).hit;
	frontTouch = GetRayCollisionBox(generateRay(translation, front), other.collisionBox).hit;
	backTouch = GetRayCollisionBox(generateRay(translation, back), other.collisionBox).hit;
	rightTouch = GetRayCollisionBox(generateRay(translation, right), other.collisionBox).hit;
	leftTouch = GetRayCollisionBox(generateRay(translation, left), other.collisionBox).hit;
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

void RigidBody3D::resolveConstrains(RigidBody3D* otherObjects, int objectCount)
{
	if (otherObjects == nullptr || objectCount <= 0)
		return;

	for (int i = 0; i < objectCount; i++)
	{
		if (&otherObjects[i] == this) continue;

		if (isCollidingWith(otherObjects[i]))
		{
			resolveCollision(otherObjects[i]);
			checkRayCollision(otherObjects[i]);
			isColliding = true;
		}
		else
		{
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

			float avgY = (velocity.y + other.velocity.y) * 0.5f;
			velocity.y = avgY;
			other.velocity.y = avgY;

			float avgX = (velocity.x + other.velocity.x) * 0.5f;
			velocity.x = avgX;
			other.velocity.x = avgX;

			float avgZ = (velocity.z + other.velocity.z) * 0.5f;
			velocity.z = avgZ;
			other.velocity.z = avgZ;

		}
		else if (!isStatic)
		{
			velocity.y = 0;
			velocity.x = 0;
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

void RigidBody3D::updateForce(float deltaTime)
{
	if (airTime > 0)
	{
		velocity.y += deltaTime;
	}

	// Apply acceleration to velocity
	velocity += acceleration * deltaTime;
	// Limit velocity to max speed
	if (Vector3Length(velocity) > maxSpeed)
	{
		velocity = Vector3Scale(Vector3Normalize(velocity), maxSpeed);
	}
	// Apply velocity to position
	translation += velocity * deltaTime;
	// Apply drag to velocity
	velocity *= drag;
	// Reset acceleration for the next frame
	acceleration = { 0, 0 };

	// Apply Touch Detection


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

}