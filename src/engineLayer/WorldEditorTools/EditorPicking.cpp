#include "EditorPicking.h"

#include <SceneManager.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
	// Below this, a direction component counts as parallel to the slab it is
	// being tested against and the reciprocal is not worth trusting.
	constexpr float kParallelEpsilon = 1.0e-6f;

	// A scale can legitimately be tiny, but a zero half-extent makes the slab
	// infinitely thin and the test meaningless.
	constexpr float kMinimumHalfExtent = 0.0001f;

	/**
	 * Slab test against a box centred on the origin.
	 *
	 * Returns the entry distance normally, and the *exit* distance when the ray
	 * starts inside the box. That second case matters in practice: an editor
	 * camera flown inside a room's collision volume would otherwise have every
	 * click swallowed at distance 0 by the walls around it, making the contents
	 * of the room unselectable.
	 */
	bool SlabTest(Vector3 origin, Vector3 direction, Vector3 halfExtents,
		float& outDistance, Vector3& outNormal)
	{
		const float o[3] = { origin.x, origin.y, origin.z };
		const float d[3] = { direction.x, direction.y, direction.z };
		const float h[3] = { halfExtents.x, halfExtents.y, halfExtents.z };

		float nearT = -FLT_MAX;
		float farT = FLT_MAX;
		int nearAxis = 0;
		int farAxis = 0;
		float nearSign = -1.0f;
		float farSign = 1.0f;
		bool solved = false;

		for (int axis = 0; axis < 3; ++axis)
		{
			const float half = fmaxf(h[axis], kMinimumHalfExtent);

			if (fabsf(d[axis]) < kParallelEpsilon)
			{
				// Running parallel to this pair of planes: the ray can only hit
				// the box if it already lies between them.
				if (o[axis] < -half || o[axis] > half) { return false; }
				continue;
			}

			const float inverse = 1.0f / d[axis];
			float entry = (-half - o[axis]) * inverse; // plane at -half, outward normal -1
			float exit = (half - o[axis]) * inverse;   // plane at +half, outward normal +1
			float entrySign = -1.0f;

			if (entry > exit)
			{
				std::swap(entry, exit);
				entrySign = 1.0f; // the ray runs backwards, so it enters through +half
			}

			if (entry > nearT) { nearT = entry; nearAxis = axis; nearSign = entrySign; }
			if (exit < farT) { farT = exit; farAxis = axis; farSign = -entrySign; }
			if (nearT > farT) { return false; }

			solved = true;
		}

		// Every axis was parallel, which only happens for a zero-length
		// direction. Nothing meaningful to report.
		if (!solved) { return false; }

		if (farT < 0.0f) { return false; } // the whole box is behind the ray

		const bool inside = nearT < 0.0f;
		const int axis = inside ? farAxis : nearAxis;

		// Face the ray either way: the entry face's outward normal when hit from
		// outside, the exit face's inward normal when starting inside.
		const float sign = inside ? -farSign : nearSign;

		outDistance = inside ? farT : nearT;
		outNormal = {
			axis == 0 ? sign : 0.0f,
			axis == 1 ? sign : 0.0f,
			axis == 2 ? sign : 0.0f
		};
		return true;
	}

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
		if (!RayOrientedBox(ray, object->rigidBody3D.translation, object->rigidBody3D.scale,
			object->rigidBody3D.rotation, distance, normal))
		{
			return;
		}

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
	const float lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y
		+ rotation.z * rotation.z + rotation.w * rotation.w;
	if (lengthSquared < 1.0e-8f) { return QuaternionIdentity(); }
	return QuaternionNormalize(rotation);
}

Ray GetEditorMouseRay(const Camera3D& camera)
{
	return GetScreenToWorldRay(GetMousePosition(), camera);
}

bool RayOrientedBox(const Ray& ray, Vector3 center, Vector3 size, Quaternion rotation,
	float& outDistance, Vector3& outNormal)
{
	if (Vector3LengthSqr(ray.direction) < kParallelEpsilon) { return false; }

	// Rather than write a dedicated OBB routine, push the ray into the object's
	// local frame and slab-test there. render3D applies scale first and then
	// rotation about the object's centre, so the inverse is: translate to the
	// centre, un-rotate, compare against half the scale.
	const Quaternion orientation = SafeRotation(rotation);
	const Quaternion inverse = QuaternionInvert(orientation);

	const Vector3 localOrigin = Vector3RotateByQuaternion(Vector3Subtract(ray.position, center), inverse);
	const Vector3 localDirection = Vector3RotateByQuaternion(Vector3Normalize(ray.direction), inverse);

	Vector3 localNormal = {};
	if (!SlabTest(localOrigin, localDirection, Vector3Scale(size, 0.5f), outDistance, localNormal))
	{
		return false;
	}

	outNormal = Vector3RotateByQuaternion(localNormal, orientation);
	return true;
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
