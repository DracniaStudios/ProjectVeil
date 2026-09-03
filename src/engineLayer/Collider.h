#pragma once
#ifndef COLLIDER_H
#define COLLIDER_H

#include <raylib.h>
#include <nlohmann/json.hpp>

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
 *
 * A zero or negative half extent inverts the volume (min > max). Both
 * CheckCollisionBoxes and the editor's ray test then report no hit, so the
 * object silently becomes uncollidable *and* unclickable with nothing on screen
 * to explain why — see the same trap documented for scale in EditorPicking.h.
 * Clamping lives in the accessors below rather than in the fields, so no route
 * into a collider (JSON, the inspector, a script) can get past it.
 */
inline constexpr float MINIMUM_COLLIDER_EXTENT = 0.01f;

/**
 * Normalises a rotation, falling back to identity when it is degenerate.
 *
 * A zeroed quaternion normalises to zero and collapses every vector it rotates
 * onto the origin — a body read before it has ever ticked would become a point
 * and pass through everything. Every route that turns a collider into world
 * space goes through here so none of them can disagree.
 */
Quaternion SafeOrientation(Quaternion rotation);

/**
 * Result of a narrow-phase contact test.
 *
 * Produced by ColliderContact() and consumed by the resolution code, so a
 * single test answers "do they touch", "which way do I push" and "how far" at
 * once rather than being recomputed for each question.
 */
struct ContactInfo
{
	bool hit = false;
	Vector3 normal = {};  // Unit length, pointing from the first body toward the second
	float depth = 0.0f;   // Overlap along `normal` — how far they must separate
};

/**
 * A collider resolved into world space, which is all the narrow phase needs.
 *
 * Separating the resolved volume from the authored Collider3D is what keeps the
 * shape maths free of any engine dependency: ColliderContact() below takes two
 * of these and knows nothing about rigid bodies, game objects or the scene. That
 * is what lets it be unit tested against raylib alone.
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
 * Box/box is the separating axis theorem: two convex shapes are apart if and
 * only if some axis exists on which their projections do not overlap, and for
 * two boxes it is enough to test 15 candidates — each box's 3 face normals plus
 * the 9 cross products of their edge directions. A gap on any one proves
 * separation and returns early; if all 15 overlap, the smallest overlap is the
 * contact normal and depth.
 *
 * The 9 cross products are not optional: without them two boxes can pass through
 * one another corner-first, which is exactly the case an axis-aligned test
 * cannot see.
 *
 * Sphere pairs are closest-point tests instead — cheaper, and exact.
 *
 * The returned normal always points from `a` toward `b`, which the resolution
 * code relies on to know which way to push.
 */
ContactInfo ColliderContact(const ColliderVolume& a, const ColliderVolume& b);

/**
 * Ray against a collider volume — exact for every shape, unlike testing the
 * axis-aligned box that encloses it.
 *
 * That difference is the whole point of using this rather than raylib's
 * GetRayCollisionBox on a body's broad-phase box: a rotated ramp, an offset
 * collider or a sphere all sit some way inside their own bounds, and a ray
 * through that gap is a miss being reported as a hit. Grounding, line of sight
 * and sound occlusion all read this, so the gap is the difference between being
 * grounded on thin air beside a wall and not.
 *
 * Returns the entry distance normally, and the EXIT distance when the ray starts
 * inside the volume — so a camera or a listener standing inside a room is not
 * swallowed at distance 0 by the walls around it. `outNormal` always faces the
 * ray: the entry face's outward normal from outside, the exit face's inward
 * normal from within.
 *
 * A zero-length direction is rejected rather than normalised.
 */
bool ColliderRaycast(const ColliderVolume& volume, Ray ray,
	float& outDistance, Vector3& outNormal);

/** Clamps every component to at least MINIMUM_COLLIDER_EXTENT. */
Vector3 SanitizeColliderSize(Vector3 size);

struct Collider3D
{
	ColliderShape shape = COLLIDER_BOX;
	ColliderMode mode = COLLIDER_COLLISION;

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

	/** Half extents along the body's own axes, clamped away from degenerate. */
	Vector3 GetLocalHalfExtents(Vector3 bodyScale) const;

	/** Centre offset in the body's local frame, before rotation. */
	Vector3 GetLocalOffset(Vector3 bodyScale) const;

	/**
	 * World radius of a sphere collider.
	 *
	 * A non-uniformly scaled sphere is an ellipsoid, which this solver has no
	 * shape for, so the largest scale component wins. That over-covers rather
	 * than under-covers: a body is never smaller to the solver than it looks.
	 */
	float GetWorldRadius(Vector3 bodyScale) const;

	/**
	 * Fits `size` and `offset` to the model's own geometry.
	 *
	 * Unions GetMeshBoundingBox over every mesh in the model. The result is in
	 * mesh-local units, which is exactly the frame `size`/`offset` live in, so it
	 * composes with Transform::scale like an authored box would.
	 *
	 * This does not reintroduce the second source of truth the tombstone comment
	 * in GameObject.cpp warns about: mesh bounds feed *into* the collider, and
	 * the collider remains the one thing SyncBroadPhaseBox() reads.
	 *
	 * A model with no meshes leaves the collider untouched rather than collapsing
	 * it to nothing.
	 */
	void FitToModel(const Model& model);
	void FitToBounds(BoundingBox bounds);

	// Save & Load Data
	Json formatToJson() const;
	bool loadFromJson(const Json& j);
};

/**
 * Resolves a collider riding on a transform into the world volume the narrow
 * phase tests.
 *
 * This is the single definition of how `size`, `offset` and `radius` compose
 * with a transform. The rigid body builds its volumes through here and so do the
 * tests, so the solver, the broad-phase box, the touch rays and the debug draw
 * can never disagree about where a collider is or how big it is.
 */
ColliderVolume MakeColliderVolume(const Collider3D& collider, Vector3 translation,
	Quaternion rotation, Vector3 scale);

/**
 * Half extents of the smallest AXIS-ALIGNED box containing the volume.
 *
 * This is the BROAD-phase size. A rotated box is represented by the smallest
 * axis-aligned volume that still contains it — the standard |R| * halfExtents
 * construction — and a sphere is its radius on all three axes. The narrow phase
 * uses the true oriented volume and does not inherit this slack.
 */
Vector3 ColliderWorldHalfExtents(const ColliderVolume& volume);

#endif
