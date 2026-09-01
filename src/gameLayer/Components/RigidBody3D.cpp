#include "Physics.h"

#include <gameMap.h>
#include <iostream>
#include <SceneManager.h>

#include <cfloat>
#include <cmath>


#pragma region Extents
ColliderVolume RigidBody3D::GetColliderVolume() const
{
	// MakeColliderVolume is the single definition of how a collider composes with
	// a transform. Everything below reads this rather than rebuilding the maths,
	// which is what stops the solver and the debug draw from drifting apart.
	return MakeColliderVolume(collider, translation, rotation, scale);
}

Vector3 RigidBody3D::GetColliderCenter() const
{
	return GetColliderVolume().center;
}

Vector3 RigidBody3D::GetWorldHalfExtents() const
{
	return ColliderWorldHalfExtents(GetColliderVolume());
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

	// Face Translation — rotation- and collider-aware, so the rays leave from the
	// faces of the volume the solver actually tests rather than from an unrotated
	// one, or from the object's origin when its collider is offset from it
	const Vector3 c = GetColliderCenter();
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

ContactInfo RigidBody3D::getContact(const RigidBody3D& other) const
{
	// The test itself lives in Collider3D.cpp, which has no engine dependency and
	// so can be unit tested against raylib alone. This is the adaptor: resolve
	// both bodies into world volumes and hand them over. The contract callers
	// rely on is unchanged — the normal comes back unit length, pointing from
	// this body toward `other`, which is the direction resolveCollision pushes
	// this body backwards along.
	return ColliderContact(GetColliderVolume(), other.GetColliderVolume());
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

	// A trigger reports contacts and applies no physics. That has to include the
	// touch flags: a trigger volume that set downTouch would be a floor the
	// player could stand and jump on, which is exactly what a damage zone or an
	// objective volume must not be. Either side being a trigger is enough — a
	// solid body must not be stopped by one any more than the reverse.
	const bool isTriggerPair = collider.isTrigger() || other->rigidBody3D.collider.isTrigger();

	// Update Collision Flags
	if (!isTriggerPair) { checkRayCollision(other->rigidBody3D); }

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
		// "First contact" means absent from last frame's contacts, not merely
		// different from the previous pair the solver happened to visit.
		const auto isKnown = [other](const std::vector<GameObject*>& contacts) {
			return std::find(contacts.begin(), contacts.end(), other) != contacts.end();
		};

		if (!isKnown(contactsThisFrame))
		{
			if (!isKnown(contactsLastFrame))
			{
				// onCollision fires for triggers too, so switching an object to a
				// trigger does not silently stop whatever already listened to it.
				// onTriggerEnter is the additional signal, not a replacement.
				self->onCollision(other);
				if (isTriggerPair) { self->onTriggerEnter(other); }
			}
			contactsThisFrame.push_back(other);
		}

		// Recorded by ID: the matching exit fires a frame later, by which time this
		// partner may no longer exist.
		if (isTriggerPair
			&& std::find(triggerContactsThisFrame.begin(), triggerContactsThisFrame.end(), other->id)
				== triggerContactsThisFrame.end())
		{
			triggerContactsThisFrame.push_back(other->id);
		}

		isColliding = true;
	}
	else
	{
		// Only this pair stopped overlapping; other contacts this frame stand.
		std::erase(contactsThisFrame, other);
		isColliding = !contactsThisFrame.empty();
		return; // nothing overlapping, nothing to separate
	}

	// A trigger has now done everything it does: it detected the overlap and
	// reported it. Nothing is pushed, no velocity changes, no friction.
	if (isTriggerPair) { return; }

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
		// Sized from the collision volume rather than the render scale: friction
		// acts at the contact face, which belongs to the collider, not the model.
		// These are the same value for a default collider, so nothing changes for a
		// body whose collider has never been touched.
		const Vector3 contactSize = Vector3Scale(GetLocalHalfExtents(), 2.0f);
		const Vector3 otherContactSize = Vector3Scale(other.GetLocalHalfExtents(), 2.0f);

		Vector3 frictionVec = tangentDir * -frictionImpulse;
		Vector3 lever = normal * (std::abs(Vector3DotProduct(contactSize, normal)) * 0.5f);
		Vector3 angularImpulse = Vector3CrossProduct(lever, frictionVec);

		if (!isStatic)
		{
			float inertia = Vector3LengthSqr(contactSize) / 6.0f; // box inertia, unit mass
			if (inertia > 0.0001f) angularVelocity += angularImpulse / inertia;
		}
		if (!other.isStatic)
		{
			float inertia = Vector3LengthSqr(otherContactSize) / 6.0f;
			if (inertia > 0.0001f) other.angularVelocity -= angularImpulse / inertia;
		}
	}

}
#pragma endregion

#pragma region Forces
void RigidBody3D::UpdateForce(float deltaTime)
{
	// Control Air Time
	if (downTouch) { airTime = 0; }
	else { 
		airTime += deltaTime;
		// Saturating the boost in Velocity to avoid excessive acceleration when falling for a long time.
		constexpr float kMaxAirTimeBoost = 50.0f; // Maximum boost to velocity due to air time
		acceleration.y -= fminf(airTime, kMaxAirTimeBoost);
	}

	// Apply acceleration to velocity
	if (!lockAcceleration) {
		velocity += acceleration * deltaTime;
	}

	// Limit velocity to max speed (squared compare avoids the sqrt)
	if (Vector3LengthSqr(velocity) > 999.0f * 999.0f)
	{
		velocity = Vector3Scale(Vector3Normalize(velocity), 999);
	}

	// Apply velocity to position
	translation += velocity * deltaTime;

	// Apply drag to velocity (normalized so damping is framerate-independent,
	// tuned to match the old per-frame factor at 60 FPS)
	if (!lockVelocity) {
		velocity *= powf(drag, deltaTime * 60.0f);
	}
	// Reset acceleration for the next frame
	acceleration = {};

	// Rebuild touch flags: clear once, then accumulate across nearby bodies.
	// The cheap AABB test gates the 6 raycasts so distant objects cost nothing.
	upTouch = downTouch = frontTouch = backTouch = leftTouch = rightTouch = false;

	const float touchMargin = 0.15f;
	const Vector3 nearCenter = GetColliderCenter();
	const Vector3 nearHalf = GetWorldHalfExtents();
	BoundingBox nearBox = {
		{ nearCenter.x - nearHalf.x - touchMargin, nearCenter.y - nearHalf.y - touchMargin, nearCenter.z - nearHalf.z - touchMargin },
		{ nearCenter.x + nearHalf.x + touchMargin, nearCenter.y + nearHalf.y + touchMargin, nearCenter.z + nearHalf.z + touchMargin }
	};

	// currentScene is null between the OUT and IN halves of a scene transition,
	// and Player::update() reaches this without Scene_updateScene's null guard.
	Scene* scene = SceneManager::getInstance().currentScene;
	if (scene == nullptr) { return; }

	for (auto& obj : scene->gameMap.gameObjects)
	{
		if (&obj.rigidBody3D == this) { continue; } // rays from our own faces always hit our own box
		if (!CheckCollisionBoxes(nearBox, obj.rigidBody3D.collisionBox)) { continue; }
		checkRayCollision(obj.rigidBody3D);
	}
}

void RigidBody3D::Update(float deltaTime)
{
	// Runs once per body per frame, before solveCollision's iterations. Rolling
	// the lists here is what makes onCollision fire on entering a contact rather
	// than every frame it persists, and it drops any partner that was destroyed
	// since — nothing carries over except through this frame's overlap tests.
	contactsLastFrame.swap(contactsThisFrame);
	contactsThisFrame.clear();

	if (!isEnabled) {
		canCollide = false;
		return;
	}
	if (!isStatic) {
		if (!lockTranslation) {
			if (useGravity) ApplyGravity();
			UpdateForce(deltaTime);
		}

		if (!lockRotation) {
			// Integrate angular velocity into the rotation quaternion
			float angSpeed = Vector3Length(angularVelocity);
			if (angSpeed > 0.001f)
			{
				Quaternion spin = QuaternionFromAxisAngle(angularVelocity / angSpeed, angSpeed * deltaTime);
				rotation = QuaternionNormalize(QuaternionMultiply(spin, rotation));
			}

			// Angular drag; settle quickly once grounded
			if (!lockAngularVelocity) {
				angularVelocity *= expf((downTouch ? -6.0f : -0.4f) * deltaTime);
			}
		}
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

void RigidBody3D::DispatchTriggerEvents(GameObject* self, GameMap* map)
{
	if (self == nullptr || map == nullptr) { return; }

	// At this point triggerContactsThisFrame still holds what the PREVIOUS
	// frame's solver accumulated, and triggerContactsLastFrame the frame before
	// that. Anything in the older set that is absent from the newer one stopped
	// overlapping during the previous frame — which is the earliest a departure
	// can possibly be known, since a single solver pass visits pairs in no
	// particular order.
	for (const std::uint64_t id : triggerContactsLastFrame)
	{
		const bool stillTouching = std::find(triggerContactsThisFrame.begin(),
			triggerContactsThisFrame.end(), id) != triggerContactsThisFrame.end();
		if (stillTouching) { continue; }

		// A destroyed partner resolves to nullptr and is simply dropped. This is
		// the whole reason these lists hold IDs rather than pointers.
		if (GameObject* other = FindWorldObjectByID(*map, id))
		{
			self->onTriggerExit(other);
		}
	}

	triggerContactsLastFrame.swap(triggerContactsThisFrame);
	triggerContactsThisFrame.clear();
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

	// Rotation- and collider-aware: the box is the smallest axis-aligned volume
	// that contains the body's collision volume. It used to be built from `scale`
	// alone, so rotating an object left its collision box pointing the old way — a
	// 45-degree wall was solid where it was not and passable where it was. It now
	// also follows a resized or offset collider, and a sphere collider contributes
	// its radius on all three axes.
	const ColliderVolume volume = GetColliderVolume();
	const Vector3 center = volume.center;
	const Vector3 extent = ColliderWorldHalfExtents(volume);

	collisionBox.min = { center.x - extent.x, center.y - extent.y, center.z - extent.z };
	collisionBox.max = { center.x + extent.x, center.y + extent.y, center.z + extent.z };
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
	j["CanCollide"] = canCollide;

	j["LockAngularVelocity"] = lockAngularVelocity;
	j["LockVelocity"] = lockVelocity;
	j["LockAcceleration"] = lockAcceleration;
	j["LockTranslation"] = lockTranslation;
	j["LockRotation"] = lockRotation;
	j["LockScale"] = lockScale;

	j["Collider"] = collider.formatToJson();

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
	canCollide = j.value("CanCollide", true);

	lockAngularVelocity = j.value("LockAngularVelocity", false);
	lockVelocity = j.value("LockVelocity", false);
	lockAcceleration = j.value("LockAcceleration", false);
	lockTranslation = j.value("LockTranslation", false);
	lockRotation = j.value("LockRotation", false);
	lockScale = j.value("LockScale", false);

	// A world saved before colliders existed has no "Collider" key at all. The
	// `*this = {}` above has already installed the default box, which reproduces
	// the old `scale`-derived volume exactly, so those saves load unchanged and
	// no save version bump is needed.
	if (j.contains("Collider")) { collider.loadFromJson(j["Collider"]); }

	lastPosition = translation;

	return true;

}
#pragma endregion