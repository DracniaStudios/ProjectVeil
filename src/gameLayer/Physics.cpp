#include "Physics.h"

#include <iostream>
#include <gameMap.h>
#include <asserts.h>
#include <GameObject.h>



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
			resolveCollision(otherObjects[i]);
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
			float avg = (velocity.y + other.velocity.y) * 0.5f;
			velocity.y = avg;
			other.velocity.y = avg;
		}
		else if (!isStatic)
		{
			velocity.y = 0;
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