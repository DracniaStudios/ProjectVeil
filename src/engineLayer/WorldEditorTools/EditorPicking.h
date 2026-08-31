#pragma once
#ifndef EDITOR_PICKING_H
#define EDITOR_PICKING_H

#include <raylib.h>
#include <raymath.h>

#include <cstdint>

struct GameObject;
struct Scene;

/**
 * What a pick query is allowed to hit.
 *
 * Selection and placement deliberately use different sets. Selection honours
 * canBeSelected, which GameMap::create() clears on the floor so a designer
 * cannot accidentally drag the world out from under themselves. Placement has
 * the exact opposite requirement — the floor is the most common surface to drop
 * an object onto — so it queries every visible solid instead. Sharing one
 * filter between them makes point-and-place unable to see the ground.
 */
enum PickFilter
{
	PICK_SELECTABLE, // Honours canBeSelected — what a left click selects
	PICK_SURFACE,    // Any visible solid — what point-and-place lands on
};

/**
 * Result of a mouse-ray query against the scene.
 *
 * `object` is valid only for the frame it was produced in. GameMap::saveObject()
 * push_backs and re-sorts gameObjects, so every pointer into that vector dies on
 * the next spawn or delete — `id` is the value worth keeping.
 */
struct PickResult
{
	bool hit = false;
	std::uint64_t id = 0;
	GameObject* object = nullptr;
	float distance = 0.0f;
	Vector3 point = {};  // World-space hit position
	Vector3 normal = {}; // Surface normal at the hit, always facing the ray
};

/** Mouse ray in world space, for the camera currently driving the viewport. */
Ray GetEditorMouseRay(const Camera3D& camera);

/**
 * Nearest hit against every object in the scene, across all three containers
 * (gameObjects, entities, interactables). `ignoreId` skips one object, which
 * placement uses so a ghost preview cannot land on itself.
 */
PickResult PickSceneObject(Scene* scene, const Ray& ray, PickFilter filter, std::uint64_t ignoreId = 0);

/**
 * Ray against an oriented box. GameObject::render3D bakes scale and rotation
 * into model.transform, so the drawn volume is an OBB — testing the axis-aligned
 * rigidBody3D.collisionBox would make a rotated object pick from a box that
 * visibly does not match it.
 */
bool RayOrientedBox(const Ray& ray, Vector3 center, Vector3 size, Quaternion rotation,
	float& outDistance, Vector3& outNormal);

/** Ray against an arbitrary plane. Returns false when near-parallel or behind. */
bool RayPlane(const Ray& ray, Vector3 planePoint, Vector3 planeNormal, Vector3& outPoint);

/** Ray against the horizontal plane at `planeY` — the placement fallback. */
bool RayGroundPlane(const Ray& ray, float planeY, Vector3& outPoint);

/**
 * Parameter of the point on the infinite line (origin + t * axis) closest to the
 * ray. This is the core of every axis drag. Returns false when the axis points
 * at the camera, where the answer is undefined and the solve divides by ~0.
 */
bool ClosestPointOnAxis(const Ray& ray, Vector3 origin, Vector3 axis, float& outT);

/** Shortest distance from a ray to the segment origin..origin+axis*length. */
float RayAxisDistance(const Ray& ray, Vector3 origin, Vector3 axis, float length);

/** Shortest distance from a ray to a point. */
float RayPointDistance(const Ray& ray, Vector3 point);

/** Rounds to the nearest multiple of `step`. A step of 0 or less is a no-op. */
float SnapValue(float value, float step);
Vector3 SnapVector(Vector3 value, float step);

/**
 * Smallest extent any editor operation may leave on an object.
 *
 * A zero or negative extent inverts the collision box (min > max). Both
 * CheckCollisionBoxes and the slab test above then report no hit, so the object
 * silently becomes unclickable in the viewport *and* uncollidable in the solver
 * — with nothing on screen to explain why. RigidBody3D::Update only repairs an
 * exactly-zero scale, so negatives have to be caught here.
 */
inline constexpr float MINIMUM_EDITOR_SCALE = 0.01f;

/** Clamps every component to at least MINIMUM_EDITOR_SCALE. */
Vector3 SanitizeScale(Vector3 scale);

/**
 * Normalizes a rotation, falling back to identity when it is degenerate.
 *
 * RigidBody3D::Update() repairs a zeroed quaternion, but the editor freezes the
 * simulation while manipulating objects, so Update() is not running. A {0,0,0,0}
 * quaternion normalises to zero and collapses every vector it rotates onto the
 * origin, which silently breaks picking and the gizmo basis.
 */
Quaternion SafeRotation(Quaternion rotation);

#endif
