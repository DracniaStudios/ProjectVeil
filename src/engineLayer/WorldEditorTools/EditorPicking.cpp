#include "EditorPicking.h"

#include <Collider.h>

#include <SceneManager.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
	// Below this, a direction component counts as parallel to the slab it is
	// being tested against and the reciprocal is not worth trusting.
	constexpr float kParallelEpsilon = 1.0e-6f;

	/**
	 * Only what is actually on screen may be clicked. A hidden or disabled
	 * object stays reachable through the Object Browser list, which is the
	 * honest place to find something you cannot see — letting the mouse pick it
	 * would mean clicking empty space and selecting an invisible object.
	 */
	bool IsPickable(const GameObject& object, PickFilter filter)
	{
		if (!object.isEnabled) { return false; }
		if (!object.display3DModel) { return false; }
		if (filter == PICK_SELECTABLE && !object.canBeSelected) { return false; }
		return true;
	}

	// Keeps `best` holding the nearest hit seen so far.
	void TestObject(GameObject* object, const Ray& ray, PickFilter filter,
		std::uint64_t ignoreId, PickResult& best)
	{
		if (object == nullptr) { return; }
		if (object->id == ignoreId) { return; }
		if (!IsPickable(*object, filter)) { return; }

		float distance = 0.0f;
		Vector3 normal = {};
		
		
		/// Collider Check
		if (!RayOrientedBox(ray, object->rigidBody3D.translation, object->rigidBody3D.scale * object->rigidBody3D.collider.size,
			object->rigidBody3D.rotation, distance, normal))
		{
			return;
		}
		
		/// Bounding Box Check
		/* 
		if (!RayOrientedBox(ray, object->rigidBody3D.translation, object->rigidBody3D.scale,
			object->rigidBody3D.rotation, distance, normal))
		{
			return;
		}
		*/


		// Strictly nearer, so the first object at a given depth wins and the
		// selection does not flicker between coplanar faces.
		if (best.hit && distance >= best.distance) { return; }

		best.hit = true;
		best.id = object->id;
		best.object = object;
		best.distance = distance;
		best.point = Vector3Add(ray.position, Vector3Scale(Vector3Normalize(ray.direction), distance));
		best.normal = normal;
	}
}

Quaternion SafeRotation(Quaternion rotation)
{
	// Forwards to the collider's guard so there is a single definition of what a
	// degenerate rotation repairs to.
	return SafeOrientation(rotation);
}

Ray GetEditorMouseRay(const Camera3D& camera)
{
	return GetScreenToWorldRay(GetMousePosition(), camera);
}

bool RayOrientedBox(const Ray& ray, Vector3 center, Vector3 size, Quaternion rotation,
	float& outDistance, Vector3& outNormal)
{
	// The ray/box maths lives with the collider in Collider3D.cpp, so the editor
	// and the game share one implementation instead of two that can drift apart.
	//
	// This still describes the VISUAL box — the object's scale and rotation, not
	// its collider — because selection has to follow what render3D draws. An
	// object whose collider was shrunk to fit its art must still be clickable
	// anywhere the art is.
	const Quaternion orientation = SafeRotation(rotation);

	ColliderVolume volume = {};
	volume.shape = COLLIDER_BOX;
	volume.center = center;
	volume.axes[0] = Vector3RotateByQuaternion(Vector3{ 1.0f, 0.0f, 0.0f }, orientation);
	volume.axes[1] = Vector3RotateByQuaternion(Vector3{ 0.0f, 1.0f, 0.0f }, orientation);
	volume.axes[2] = Vector3RotateByQuaternion(Vector3{ 0.0f, 0.0f, 1.0f }, orientation);
	volume.halfExtents = Vector3Scale(size, 0.5f);

	return ColliderRaycast(volume, ray, outDistance, outNormal);
}

PickResult PickSceneObject(Scene* scene, const Ray& ray, PickFilter filter, std::uint64_t ignoreId)
{
	PickResult best = {};
	if (scene == nullptr) { return best; }

	// Index rather than a range-for: TestObject only reads, but iterating a
	// vector that anything else could grow is the habit this file avoids.
	for (size_t i = 0; i < scene->gameMap.gameObjects.size(); ++i)
	{
		TestObject(&scene->gameMap.gameObjects[i], ray, filter, ignoreId, best);
	}

	for (auto& [id, entity] : scene->gameMap.entities)
	{
		TestObject(entity.get(), ray, filter, ignoreId, best);
	}

	for (auto& [id, interactable] : scene->gameMap.interactables)
	{
		TestObject(interactable.get(), ray, filter, ignoreId, best);
	}

	return best;
}

bool RayPlane(const Ray& ray, Vector3 planePoint, Vector3 planeNormal, Vector3& outPoint)
{
	const Vector3 direction = Vector3Normalize(ray.direction);
	const Vector3 normal = Vector3Normalize(planeNormal);
	const float denominator = Vector3DotProduct(direction, normal);

	// Near-parallel: the intersection races off toward infinity and the result
	// is either astronomically large or NaN. Either one poisons the rigid body
	// it is written into, and NaN then spreads through the solver to every
	// object that touches it — so refuse rather than clamp.
	if (fabsf(denominator) < 1.0e-4f) { return false; }

	const float distance = Vector3DotProduct(Vector3Subtract(planePoint, ray.position), normal) / denominator;
	if (distance < 0.0f) { return false; } // the plane is behind the camera

	outPoint = Vector3Add(ray.position, Vector3Scale(direction, distance));
	return true;
}

bool RayGroundPlane(const Ray& ray, float planeY, Vector3& outPoint)
{
	return RayPlane(ray, Vector3{ 0.0f, planeY, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f }, outPoint);
}

bool ClosestPointOnAxis(const Ray& ray, Vector3 origin, Vector3 axis, float& outT)
{
	if (Vector3LengthSqr(axis) < kParallelEpsilon) { return false; }
	if (Vector3LengthSqr(ray.direction) < kParallelEpsilon) { return false; }

	const Vector3 u = Vector3Normalize(axis);
	const Vector3 v = Vector3Normalize(ray.direction);

	const float b = Vector3DotProduct(u, v);
	const float denominator = 1.0f - b * b;

	// The axis points (nearly) straight at the camera. Its projection collapses
	// to a dot, so there is no screen direction to drag along and the solve
	// divides by ~0. Refusing to move is the correct answer.
	if (fabsf(denominator) < 1.0e-4f) { return false; }

	const Vector3 w = Vector3Subtract(origin, ray.position);
	const float d = Vector3DotProduct(u, w);
	const float e = Vector3DotProduct(v, w);

	outT = (b * e - d) / denominator;
	return true;
}

float RayAxisDistance(const Ray& ray, Vector3 origin, Vector3 axis, float length)
{
	float t = 0.0f;
	if (!ClosestPointOnAxis(ray, origin, axis, t)) { return FLT_MAX; }

	// Clamp onto the drawn handle: the maths above works on an infinite line,
	// but only the segment the user can see is grabbable.
	t = Clamp(t, 0.0f, length);
	return RayPointDistance(ray, Vector3Add(origin, Vector3Scale(Vector3Normalize(axis), t)));
}

float RayPointDistance(const Ray& ray, Vector3 point)
{
	if (Vector3LengthSqr(ray.direction) < kParallelEpsilon) { return FLT_MAX; }

	const Vector3 direction = Vector3Normalize(ray.direction);
	float along = Vector3DotProduct(Vector3Subtract(point, ray.position), direction);
	if (along < 0.0f) { along = 0.0f; } // behind the camera: measure from the eye

	return Vector3Distance(point, Vector3Add(ray.position, Vector3Scale(direction, along)));
}

float SnapValue(float value, float step)
{
	if (step <= 0.0f) { return value; }
	return roundf(value / step) * step;
}

Vector3 SnapVector(Vector3 value, float step)
{
	return { SnapValue(value.x, step), SnapValue(value.y, step), SnapValue(value.z, step) };
}

Vector3 SanitizeScale(Vector3 scale)
{
	return {
		fmaxf(scale.x, MINIMUM_EDITOR_SCALE),
		fmaxf(scale.y, MINIMUM_EDITOR_SCALE),
		fmaxf(scale.z, MINIMUM_EDITOR_SCALE)
	};
}
