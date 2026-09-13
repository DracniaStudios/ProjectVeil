#pragma once
#ifndef COLLIDER_H
#define COLLIDER_H

#include <raylib.h>
#include <nlohmann/json.hpp>
#include <iostream>

using Json = nlohmann::json;

/**
 * The collision volume of a body, authored independently of its render scale.
 *
 * Before this existed the collision shape *was* Transform::scale — a body's
 * half extents were literally `scale * 0.5f`. That made two separate things one
 * number: a door 0.2 units thick had to be *drawn* 0.2 thick to *collide* 0.2
 * thick, and there was no way to say "this volume reports overlap but does not
 * push back".
 *
 * A Collider3D answers three questions the transform alone could not:
 *   - what SHAPE is this body to the solver (box, sphere, or the loaded mesh's
 *     own bounds),
 *   - how BIG and WHERE, relative to the transform it rides on,
 *   - and in which MODE — does a contact produce a physics response, or only an
 *     event.
 */

#pragma region Enums
enum ColliderShape
{
	COLLIDER_BOX,     // Oriented box, sized by `size` * Transform::scale
	COLLIDER_SPHERE,  // Sphere, radius `radius` * the largest scale component
	COLLIDER_MESH,    // Box auto-fitted to the loaded model's own bounds
	COLLIDER_SHAPE_COUNT
};

/**
 * What a contact does.
 *
 * COLLISION resolves through the rigid body: positional correction, normal
 * impulse, friction, and the touch flags that drive grounding.
 *
 * TRIGGER detects and reports the same contact and then stops. Nothing is
 * pushed, no velocity changes, and crucially no touch flag is set — a trigger
 * that set `downTouch` would be a floor you could jump off, which is exactly
 * what a damage zone or an objective volume must not be.
 */
enum ColliderMode
{
	COLLIDER_COLLISION,
	COLLIDER_TRIGGER,
	COLLIDER_MODE_COUNT
};

inline const char* colliderShapeToString(int shape)
{
	switch (shape)
	{
	case COLLIDER_SPHERE: return "Sphere";
	case COLLIDER_MESH:   return "Mesh";
	default:              return "Box";
	}
}

inline const char* colliderModeToString(int mode)
{
	switch (mode)
	{
	case COLLIDER_TRIGGER: return "Trigger";
	default:               return "Collision";
	}
}
#pragma endregion

/**
 * Smallest extent a collider may present to the solver.
 */
inline constexpr float MINIMUM_COLLIDER_EXTENT = 0.01f;

/**
 * Normalises a rotation, falling back to identity when it is degenerate.
 */
Quaternion SafeOrientation(Quaternion rotation);

/**
 * Result of a narrow-phase contact test.
 */
struct ContactInfo
{
	bool hit = false;
	Vector3 normal = {};  // Unit length, pointing from the first body toward the second
	float depth = 0.0f;   // Overlap along `normal` — how far they must separate
};

/**
 * A collider resolved into world space, which is all the narrow phase needs.
 */
struct ColliderVolume
{
	ColliderShape shape = COLLIDER_BOX;

	Vector3 center = {};                                  // World-space centre of the volume
	Vector3 axes[3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} }; // Unit, the body's rotated local axes
	Vector3 halfExtents = {};                             // Box/mesh, along `axes`
	float radius = 0.0f;                                  // Sphere
};

/**
 * Narrow phase for every supported shape pair.
 * 
 * The 9 cross products are not optional: without them two boxes can pass through
 * one another corner-first, which is exactly the case an axis-aligned test
 * cannot see.
 */
ContactInfo ColliderContact(const ColliderVolume& a, const ColliderVolume& b);

/**
 * Ray against a collider volume — exact for every shape, unlike testing the
 * axis-aligned box that encloses it.
 *
 * A zero-length direction is rejected rather than normalised.
 */
bool ColliderRaycast(const ColliderVolume& volume, Ray ray,
	float& outDistance, Vector3& outNormal);

/** Clamps every component to at least MINIMUM_COLLIDER_EXTENT. */
Vector3 SanitizeColliderSize(Vector3 size);

struct Collider3D
{
private:
	uint64_t objectID = -1;
	bool canSetID = true;
public:
	int SetObjectID(uint64_t id = 0) { if (canSetID) { objectID = id; canSetID = false; return objectID; } else return objectID; }
	int GetObjectID() const { return objectID; }
	ColliderShape shape = COLLIDER_BOX;
	ColliderMode mode = COLLIDER_COLLISION;
	bool canCollide = true;
	/**
	 * Size and offset are LOCAL units, multiplied by the body's Transform::scale
	 * — the same composition render3D performs when it bakes
	 * MatrixScale(scale) * QuaternionToMatrix(rotation) into model.transform. So
	 * scaling an object scales its collider with it, and a collider authored
	 * against a model stays correct at any size.
	 *
	 * These defaults are load bearing. With size {1,1,1} and offset {0,0,0} the
	 * local half extents come out as exactly `scale * 0.5f` — the expression the
	 * body used before colliders existed. That is what makes every save written
	 * before this feature behave identically after it.
	 */
	Vector3 size = { 1.0f, 1.0f, 1.0f };
	Vector3 offset = { 0.0f, 0.0f, 0.0f };
	float radius = 0.5f; // Sphere only

	bool isTrigger() const { return mode == COLLIDER_TRIGGER; }

	virtual bool onCollisionEnter(Collider3D& other) const;
	virtual bool onCollisionExit(Collider3D& other) const;
	virtual bool onTriggerEnter(Collider3D& other) const;
	virtual bool onTriggerExit(Collider3D& other) const;

	/** Half extents along the body's own axes, clamped away from degenerate. */
	Vector3 GetLocalHalfExtents(Vector3 bodyScale) const;

	/** Centre offset in the body's local frame, before rotation. */
	Vector3 GetLocalOffset(Vector3 bodyScale) const;

	/**
	 * World radius of a sphere collider.
	 */
	float GetWorldRadius(Vector3 bodyScale) const;

	/**
	 * Fits `size` and `offset` to the model's own geometry.
	 */
	void FitToModel(const Model& model);
	void FitToBounds(BoundingBox bounds);

	// Save & Load Data
	Json formatToJson() const;
	bool loadFromJson(const Json& j);
};

ColliderVolume MakeColliderVolume(const Collider3D& collider, Vector3 translation,
	Quaternion rotation, Vector3 scale);

Vector3 ColliderWorldHalfExtents(const ColliderVolume& volume);

#endif
