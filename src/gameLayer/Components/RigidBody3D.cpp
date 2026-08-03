#include "Physics.h"

#include <gameMap.h>
#include <iostream>
#include <SceneManager.h>

#include <cfloat>
#include <cmath>


namespace
{
	// A zeroed quaternion normalises to zero and would collapse a body to a
	// point, making it pass through everything. SyncCollisionBox repairs the
	// stored value, but a body can be read before it has ever ticked.
	Quaternion SafeOrientation(Quaternion rotation)
	{
		const float lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y
			+ rotation.z * rotation.z + rotation.w * rotation.w;
		if (lengthSquared < 1.0e-8f) { return QuaternionIdentity(); }
		return QuaternionNormalize(rotation);
	}
}

#pragma region Extents
void RigidBody3D::GetWorldAxes(Vector3 axes[3]) const
{
	const Quaternion orientation = SafeOrientation(rotation);

	// Rotation preserves length, so these come out unit length — which the
	// projection maths below relies on.
	axes[0] = Vector3RotateByQuaternion(Vector3{ 1.0f, 0.0f, 0.0f }, orientation);
	axes[1] = Vector3RotateByQuaternion(Vector3{ 0.0f, 1.0f, 0.0f }, orientation);
	axes[2] = Vector3RotateByQuaternion(Vector3{ 0.0f, 0.0f, 1.0f }, orientation);
}

Vector3 RigidBody3D::GetWorldHalfExtents() const
{
	const Vector3 half = GetLocalHalfExtents();

	Vector3 axes[3];
	GetWorldAxes(axes);

	// Sum the absolute contribution of each rotated local axis to each world
	// axis. This is the standard |R| * halfExtents construction, written through
	// Vector3RotateByQuaternion so it cannot disagree with the QuaternionToMatrix
	// path render3D draws through, and so it does not depend on raylib's matrix
	// storage order.
	return {
		fabsf(axes[0].x) * half.x + fabsf(axes[1].x) * half.y + fabsf(axes[2].x) * half.z,
		fabsf(axes[0].y) * half.x + fabsf(axes[1].y) * half.y + fabsf(axes[2].y) * half.z,
		fabsf(axes[0].z) * half.x + fabsf(axes[1].z) * half.y + fabsf(axes[2].z) * half.z
	};
}

void DrawOrientedBoxWires(Vector3 center, Vector3 size, Quaternion rotation, Color color)
{
	const Quaternion orientation = SafeOrientation(rotation);
	const Vector3 half = Vector3Scale(size, 0.5f);

	// Eight corners of the local box, rotated into world space. Each index bit
	// selects the sign of one axis.
	Vector3 corner[8] = {};
	for (int index = 0; index < 8; ++index)
	{
		const Vector3 local = {
			(index & 1) ? half.x : -half.x,
			(index & 2) ? half.y : -half.y,
			(index & 4) ? half.z : -half.z
		};
		corner[index] = Vector3Add(center, Vector3RotateByQuaternion(local, orientation));
	}

	// Index pairs differing by exactly one bit are exactly the 12 edges
	static constexpr int edges[12][2] = {
		{0,1},{2,3},{4,5},{6,7},
		{0,2},{1,3},{4,6},{5,7},
		{0,4},{1,5},{2,6},{3,7}
	};

	for (const auto& edge : edges)
	{
		DrawLine3D(corner[edge[0]], corner[edge[1]], color);
	}
}
#pragma endregion

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

	// Face Translation — rotation-aware, so the rays leave from the faces of the
	// volume the solver actually tests rather than from an unrotated one
	Vector3 c = translation;
	const Vector3 half = GetWorldHalfExtents();
	float hx = half.x;
	float hy = half.y;
	float hz = half.z;

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

namespace
{
	// Below this, a cross product of two box edges is degenerate: the edges are
	// parallel, so the axis carries nothing the face axes have not already
	// covered. Normalising it would divide by ~0 and manufacture a random normal.
	constexpr float kAxisEpsilon = 1.0e-6f;

	// Cross-product axes must beat the best face axis by this margin to be
	// chosen as the contact normal. A box resting flat produces several axes
	// within floating-point noise of each other, and picking an edge normal
	// there shoves the box sideways instead of up — visible as resting jitter.
	constexpr float kFaceAxisPreference = 1.05f;

	// Half-width of an oriented box's shadow on a unit axis: the sum of each
	// local axis's extent projected onto it.
	float ProjectedRadius(const Vector3 axes[3], Vector3 half, Vector3 axis)
	{
		return fabsf(Vector3DotProduct(axis, axes[0])) * half.x
			+ fabsf(Vector3DotProduct(axis, axes[1])) * half.y
			+ fabsf(Vector3DotProduct(axis, axes[2])) * half.z;
	}
}

ContactInfo RigidBody3D::getContact(const RigidBody3D& other) const
{
	ContactInfo contact = {};

	Vector3 axesA[3];
	Vector3 axesB[3];
	GetWorldAxes(axesA);
	other.GetWorldAxes(axesB);

	const Vector3 halfA = GetLocalHalfExtents();
	const Vector3 halfB = other.GetLocalHalfExtents();
	const Vector3 delta = Vector3Subtract(other.getCenter(), getCenter());

	// The 15 candidates: 3 face normals from each box, then the 9 edge-edge
	// cross products. Face axes come first so that ties resolve to them.
	Vector3 candidates[15];
	int count = 0;
	for (int i = 0; i < 3; ++i) { candidates[count++] = axesA[i]; }
	for (int i = 0; i < 3; ++i) { candidates[count++] = axesB[i]; }
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			candidates[count++] = Vector3CrossProduct(axesA[i], axesB[j]);
		}
	}

	float bestScore = FLT_MAX; // biased, decides which axis wins
	float bestDepth = 0.0f;    // true overlap on that axis, used to separate
	Vector3 bestAxis = {};

	for (int i = 0; i < count; ++i)
	{
		Vector3 axis = candidates[i];

		const float lengthSquared = Vector3LengthSqr(axis);
		if (lengthSquared < kAxisEpsilon) { continue; } // parallel edges

		axis = Vector3Scale(axis, 1.0f / sqrtf(lengthSquared));

		const float distance = fabsf(Vector3DotProduct(delta, axis));
		const float overlap = ProjectedRadius(axesA, halfA, axis)
			+ ProjectedRadius(axesB, halfB, axis)
			- distance;

		// A gap on any single axis proves the boxes are apart. That is the whole
		// theorem, and it lets the common non-touching case leave early.
		if (overlap <= 0.0f) { return contact; }

		const float score = (i >= 6) ? overlap * kFaceAxisPreference : overlap;
		if (score < bestScore)
		{
			bestScore = score;
			bestDepth = overlap;
			bestAxis = axis;
		}
	}

	// Every candidate degenerate — only reachable with a collapsed body
	if (bestScore == FLT_MAX) { return contact; }

	// The resolution code pushes `this` backwards along the normal, so it has to
	// point from this body toward the other.
	if (Vector3DotProduct(delta, bestAxis) < 0.0f) { bestAxis = Vector3Negate(bestAxis); }

	contact.hit = true;
	contact.normal = bestAxis;
	contact.depth = bestDepth;
	return contact;
}

bool RigidBody3D::isCollidingWith(const RigidBody3D& other) const
{
	return getContact(other).hit;
}

Vector3 RigidBody3D::getCollisionNormal(const RigidBody3D& other) const
{
	return getContact(other).normal;
}

float RigidBody3D::getPenetrationDepth(const RigidBody3D& other) const
{
	return getContact(other).depth;
}

// ─── Constraint Resolution ─────────────────────────────────────────────────

void RigidBody3D::resolveConstrains(GameObject* self, GameObject* other)
{
	if (&other->rigidBody3D == this) return;
	if (other->rigidBody3D.canCollide == false || self->rigidBody3D.canCollide == false) return;

	// Update Collision Flags
	checkRayCollision(other->rigidBody3D);

	// One separating-axis test drives both the event and the resolution. The
	// narrow phase walks 15 axes and the solver runs 8 iterations over every
	// overlapping pair, so asking isColliding/normal/depth separately — as this
	// used to — paid for four full tests where one does.
	const ContactInfo contact = getContact(other->rigidBody3D);

	if (contact.hit)
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
		return; // nothing overlapping, nothing to separate
	}

	// Reset Position to prevent tunneling
	resolveCollision(other->rigidBody3D, contact);
}

void RigidBody3D::resolveCollision(RigidBody3D& other)
{
	// Standalone entry point: runs its own narrow phase, then defers below
	const ContactInfo contact = getContact(other);
	if (contact.hit) { resolveCollision(other, contact); }
}

void RigidBody3D::resolveCollision(RigidBody3D& other, const ContactInfo& contact)
{
	if (!contact.hit) return;
	if (isStatic && other.isStatic) return;

	// If this is static, let the dynamic body handle it to keep logic in one
	// place. The normal points from this body toward `other`, so swapping the
	// roles has to flip it — otherwise the dynamic body is pushed the wrong way,
	// straight through the static one it just hit.
	if (isStatic && !other.isStatic)
	{
		ContactInfo flipped = contact;
		flipped.normal = Vector3Negate(contact.normal);
		other.resolveCollision(*this, flipped);
		return;
	}

	const Vector3 normal = contact.normal;
	const float penetration = contact.depth;

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
	else
	{
		airTime += deltaTime;

		// Falling feels snappier the longer a body free-falls, but the boost was
		// unbounded: velocity grows roughly with airTime^2, so a long fall (e.g.
		// off the map into open space) reaches the 999 units/s clamp below in
		// well under a minute. At that speed a single frame's movement can
		// exceed the thickness of typical floor/wall geometry, and collision
		// here is a discrete per-frame test, so the body tunnels straight
		// through instead of stopping. Saturating the boost keeps the effect
		// without letting acceleration run away.
		constexpr float kMaxAirTimeGravityBoost = 5.0f;
		acceleration.y -= fminf(airTime, kMaxAirTimeGravityBoost);
	}

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
	const Vector3 nearHalf = GetWorldHalfExtents();
	BoundingBox nearBox = {
		{ translation.x - nearHalf.x - touchMargin, translation.y - nearHalf.y - touchMargin, translation.z - nearHalf.z - touchMargin },
		{ translation.x + nearHalf.x + touchMargin, translation.y + nearHalf.y + touchMargin, translation.z + nearHalf.z + touchMargin }
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
	SyncCollisionBox();
}

void RigidBody3D::SyncCollisionBox()
{
	// The same degenerate-value repairs Update() performs before integrating.
	// They are repeated here because the editor's frozen path never reaches
	// Update(), and a zero scale produces an inverted collision box (min > max)
	// that both CheckCollisionBoxes and the editor's ray test read as "no hit" —
	// the object silently becomes unclickable and uncollidable.
	if (scale == Vector3Zero()) { scale = Vector3One(); }
	if (rotation == Quaternion{ 0, 0, 0, 0 }) { rotation = QuaternionIdentity(); }

	// Rotation-aware: the box is the smallest axis-aligned volume that contains
	// the rotated body. It used to be built from `scale` alone, so rotating an
	// object left its collision box pointing the old way — a 45-degree wall was
	// solid where it was not and passable where it was.
	const Vector3 extent = GetWorldHalfExtents();

	collisionBox.min = { translation.x - extent.x, translation.y - extent.y, translation.z - extent.z };
	collisionBox.max = { translation.x + extent.x, translation.y + extent.y, translation.z + extent.z };
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