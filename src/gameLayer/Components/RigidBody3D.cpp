#include "Physics.h"

#include <gameMap.h>
#include <iostream>
#include <SceneManager.h>


#pragma region Collision
void RigidBody3D::checkRayCollision(const RigidBody3D& other)
{
	// Auto function to Create a Ray
	auto generateRay = [&](Vector3 position, Vector3 direciton)
		{
			Ray newRay{};
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

	// Check each face with a raycast and accumulate touch flags so testing
	// against several bodies doesn't overwrite earlier hits (cleared once
	// per frame in UpdateForce)
	upTouch |= hitWithinRange(generateRay({ c.x, c.y + hy, c.z }, up));
	downTouch |= hitWithinRange(generateRay({ c.x, c.y - hy, c.z }, down));
	frontTouch |= hitWithinRange(generateRay({ c.x, c.y, c.z + hz }, forward));
	backTouch |= hitWithinRange(generateRay({ c.x, c.y, c.z - hz }, back));
	rightTouch |= hitWithinRange(generateRay({ c.x + hx, c.y, c.z }, right));
	leftTouch |= hitWithinRange(generateRay({ c.x - hx, c.y, c.z }, left));
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

void RigidBody3D::resolveConstrains(GameObject* self, GameObject* other)
{
	if (&other->rigidBody3D == this) return;
	if (other->rigidBody3D.canCollide == false || self->rigidBody3D.canCollide == false) return;

	// Update Collision Flags
	checkRayCollision(other->rigidBody3D);

	if (isCollidingWith(other->rigidBody3D))
	{
		// Call Collision Event on Owner only on first contact with this object;
		// the solver runs several iterations per frame and damage/events must
		// not fire on every pass. Type-specific reactions (e.g. projectile
		// damage) live in onCollision overrides so no unchecked casts happen here.
		if (collidingWith != other)
		{
			self->onCollision(other);
		}

		// Set colliding with to other object's owner
		collidingWith = other;
		isColliding = true;
	}
	else
	{
		collidingWith = nullptr;
		isColliding = false;
	}

	// Reset Position to prevent tunneling
	resolveCollision(other->rigidBody3D);
}

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
	// normal points from this body toward other, and relativeVelocity is
	// this-minus-other, so a POSITIVE dot product means the bodies are
	// closing in on each other
	Vector3 relativeVelocity = velocity - other.velocity;
	float velocityAlongNormal = Vector3DotProduct(relativeVelocity, normal);

	// Only respond if objects are still moving toward each other
	if (velocityAlongNormal <= 0) return;

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

		// Friction acts at the contact face, not the center of mass, so it
		// applies a torque that makes bodies tumble (visual only — the
		// collision box stays axis-aligned)
		Vector3 frictionVec = tangentDir * -frictionImpulse;
		Vector3 lever = normal * (std::abs(Vector3DotProduct(scale, normal)) * 0.5f);
		Vector3 angularImpulse = Vector3CrossProduct(lever, frictionVec);

		if (!isStatic)
		{
			float inertia = Vector3LengthSqr(scale) / 6.0f; // box inertia, unit mass
			if (inertia > 0.0001f) angularVelocity += angularImpulse / inertia;
		}
		if (!other.isStatic)
		{
			float inertia = Vector3LengthSqr(other.scale) / 6.0f;
			if (inertia > 0.0001f) other.angularVelocity -= angularImpulse / inertia;
		}
	}

}
#pragma endregion

#pragma region Forces
void RigidBody3D::UpdateForce(float deltaTime)
{
	auto gameMap = &SceneManager::getInstance().currentScene->gameMap;

	// Control Air Time
	if (downTouch) { airTime = 0; }
	else { airTime += deltaTime; }

	// Apply acceleration to velocity
	velocity += acceleration * deltaTime;

	// Limit velocity to max speed (squared compare avoids the sqrt)
	if (Vector3LengthSqr(velocity) > 999.0f * 999.0f)
	{
		velocity = Vector3Scale(Vector3Normalize(velocity), 999);
	}

	// Apply velocity to position
	translation += velocity * deltaTime;

	// Apply drag to velocity (normalized so damping is framerate-independent,
	// tuned to match the old per-frame factor at 60 FPS)
	velocity *= powf(drag, deltaTime * 60.0f);

	// Reset acceleration for the next frame
	acceleration = {};

	// Rebuild touch flags: clear once, then accumulate across nearby bodies.
	// The cheap AABB test gates the 6 raycasts so distant objects cost nothing.
	upTouch = downTouch = frontTouch = backTouch = leftTouch = rightTouch = false;

	const float touchMargin = 0.15f;
	BoundingBox nearBox = {
		{ translation.x - scale.x * 0.5f - touchMargin, translation.y - scale.y * 0.5f - touchMargin, translation.z - scale.z * 0.5f - touchMargin },
		{ translation.x + scale.x * 0.5f + touchMargin, translation.y + scale.y * 0.5f + touchMargin, translation.z + scale.z * 0.5f + touchMargin }
	};

	for (auto& obj : gameMap->gameObjects)
	{
		if (&obj.rigidBody3D == this) { continue; } // rays from our own faces always hit our own box
		if (!CheckCollisionBoxes(nearBox, obj.rigidBody3D.collisionBox)) { continue; }
		checkRayCollision(obj.rigidBody3D);
	}
}

void RigidBody3D::Update(float deltaTime)
{
	if (scale == Vector3Zero()) { scale = Vector3One(); }
	if (rotation == Quaternion{ 0, 0, 0, 0 }) { rotation = QuaternionIdentity(); }
	if (!isEnabled) {
		canCollide = false;
		return;
	}
	if (!isStatic) {
		if (useGravity) ApplyGravity();
		UpdateForce(deltaTime);

		// Integrate angular velocity into the rotation quaternion
		float angSpeed = Vector3Length(angularVelocity);
		if (angSpeed > 0.001f)
		{
			Quaternion spin = QuaternionFromAxisAngle(angularVelocity / angSpeed, angSpeed * deltaTime);
			rotation = QuaternionNormalize(QuaternionMultiply(spin, rotation));
		}

		// Angular drag; settle quickly once grounded
		angularVelocity *= expf((downTouch ? -6.0f : -0.4f) * deltaTime);
	}
	else
	{
		velocity = {};
		acceleration = {};
		angularVelocity = {};
	}
	lastPosition = translation;

	// Update collision box to match current position and scale
	collisionBox.min = { translation.x - scale.x * 0.5f, translation.y - scale.y * 0.5f, translation.z - scale.z * 0.5f };
	collisionBox.max = { translation.x + scale.x * 0.5f, translation.y + scale.y * 0.5f, translation.z + scale.z * 0.5f };
}

void RigidBody3D::ApplyGravity()
{
	acceleration -= appliedGravity;
}

void RigidBody3D::Teleport(Vector3 newPosition)
{
	translation = newPosition;
	lastPosition = newPosition;
}
void RigidBody3D::Teleport(float x, float y, float z) { Teleport(Vector3(x, y, z)); }

void RigidBody3D::AddForce(Vector3 forceDirection, float force)
{
	velocity += Vector3Scale(
		forceDirection,
		force
	);
}

void RigidBody3D::Jump(float force)
{
	if (!downTouch) return;
	velocity.y = force;
}

#pragma endregion

#pragma region SaveData

Json RigidBody3D::formatToJson()
{
	Json j;

	j["PosX"] = translation.x;
	j["PosY"] = translation.y;
	j["PosZ"] = translation.z;

	j["ScaleX"] = scale.x;
	j["ScaleY"] = scale.y;
	j["ScaleZ"] = scale.z;

	j["RotX"] = rotation.x;
	j["RotY"] = rotation.y;
	j["RotZ"] = rotation.z;
	j["RotW"] = rotation.w;

	j["VelocityX"] = velocity.x;
	j["VelocityY"] = velocity.y;
	j["VelocityZ"] = velocity.z;

	j["IsStatic"] = isStatic;
	j["IsEnabled"] = isEnabled;

	return j;
}

bool RigidBody3D::loadFromJson(const Json& j)
{
	*this = {};

	if (!j.contains("PosX") || !j["PosX"].is_number()) { return false; }
	if (!j.contains("PosY") || !j["PosY"].is_number()) { return false; }
	if (!j.contains("PosZ") || !j["PosZ"].is_number()) { return false; }
	
	translation.x = j["PosX"];
	translation.y = j["PosY"];
	translation.z = j["PosZ"];

	if (!j.contains("ScaleX") || !j["ScaleX"].is_number()) { return false; }
	if (!j.contains("ScaleY") || !j["ScaleY"].is_number()) { return false; }
	if (!j.contains("ScaleZ") || !j["ScaleZ"].is_number()) { return false; }

	scale.x = j["ScaleX"];
	scale.y = j["ScaleY"];
	scale.z = j["ScaleZ"];

	rotation.x = j.value("RotX", 0.0f);
	rotation.y = j.value("RotY", 0.0f);
	rotation.z = j.value("RotZ", 0.0f);
	rotation.w = j.value("RotW", 1.0f);

	if (j.contains("VelocityX") && j["VelocityX"].is_number()) { velocity.x = j["VelocityX"]; }
	if (j.contains("VelocityY") && j["VelocityY"].is_number()) { velocity.y = j["VelocityY"]; }
	if (j.contains("VelocityZ") && j["VelocityZ"].is_number()) { velocity.z = j["VelocityZ"]; }

	isStatic = j.value("IsStatic", false);
	isEnabled = j.value("IsEnabled", true);

	lastPosition = translation;

	return true;

}
#pragma endregion