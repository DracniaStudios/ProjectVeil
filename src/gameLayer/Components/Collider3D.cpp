#include <Collider.h>

#include <raymath.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

/**
 * Deliberately free of every engine dependency.
 *
 * RigidBody3D.cpp reaches SceneManager::getInstance().currentScene inside
 * UpdateForce, so it cannot be linked on its own. Keeping the shape maths here
 * instead means tests/ColliderTests.cpp links against raylib alone — the cheap
 * tier that tests/SoundFieldTests.cpp lives in — rather than dragging in the
 * whole game and a GL context. Do not include a game header in this file.
 */

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

	// A negative extent inverts the volume, which reads as "no hit" everywhere
	// and makes a body silently uncollidable. Zero is the same story one step
	// less obvious. Positive values pass through untouched, so no currently
	// valid collider changes size on its way to the solver.
	float RepairExtent(float value)
	{
		return (value > 0.0f) ? value : MINIMUM_COLLIDER_EXTENT;
	}

	// Half-width of an oriented box's shadow on a unit axis: the sum of each
	// local axis's extent projected onto it.
	float ProjectedRadius(const ColliderVolume& volume, Vector3 axis)
	{
		return fabsf(Vector3DotProduct(axis, volume.axes[0])) * volume.halfExtents.x
			+ fabsf(Vector3DotProduct(axis, volume.axes[1])) * volume.halfExtents.y
			+ fabsf(Vector3DotProduct(axis, volume.axes[2])) * volume.halfExtents.z;
	}

	// Below this, a direction component counts as parallel to the slab it is
	// being tested against and the reciprocal is not worth trusting.
	constexpr float kParallelEpsilon = 1.0e-6f;

	// A collider can legitimately be tiny, but a zero half-extent makes the slab
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
	 *
	 * Moved here from EditorPicking.cpp so the editor's ray test and the game's
	 * share one implementation rather than two that can drift.
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

	ContactInfo BoxBoxContact(const ColliderVolume& a, const ColliderVolume& b)
	{
		ContactInfo contact = {};

		const Vector3 delta = Vector3Subtract(b.center, a.center);

		// The 15 candidates: 3 face normals from each box, then the 9 edge-edge
		// cross products. Face axes come first so that ties resolve to them.
		Vector3 candidates[15];
		int count = 0;
		for (int i = 0; i < 3; ++i) { candidates[count++] = a.axes[i]; }
		for (int i = 0; i < 3; ++i) { candidates[count++] = b.axes[i]; }
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				candidates[count++] = Vector3CrossProduct(a.axes[i], b.axes[j]);
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
			const float overlap = ProjectedRadius(a, axis) + ProjectedRadius(b, axis) - distance;

			// A gap on any single axis proves the boxes are apart. That is the
			// whole theorem, and it lets the common non-touching case leave early.
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

		// The resolution code pushes the first body backwards along the normal,
		// so it has to point from that body toward the second.
		if (Vector3DotProduct(delta, bestAxis) < 0.0f) { bestAxis = Vector3Negate(bestAxis); }

		contact.hit = true;
		contact.normal = bestAxis;
		contact.depth = bestDepth;
		return contact;
	}

	ContactInfo SphereSphereContact(const ColliderVolume& a, const ColliderVolume& b)
	{
		ContactInfo contact = {};

		const Vector3 delta = Vector3Subtract(b.center, a.center);
		const float distanceSquared = Vector3LengthSqr(delta);
		const float reach = a.radius + b.radius;

		// Exactly touching counts as apart, matching the box path's `overlap <= 0`
		if (distanceSquared >= reach * reach) { return contact; }

		const float distance = sqrtf(distanceSquared);

		contact.hit = true;
		contact.depth = reach - distance;
		// Concentric spheres have no separating direction at all. Pick one rather
		// than dividing by ~0 and handing the solver a NaN to push along.
		contact.normal = (distance > kAxisEpsilon)
			? Vector3Scale(delta, 1.0f / distance)
			: Vector3{ 0.0f, 1.0f, 0.0f };
		return contact;
	}

	/**
	 * Closest point on the oriented box to the sphere centre.
	 *
	 * The returned normal points FROM THE SPHERE TOWARD THE BOX. The caller is
	 * responsible for flipping it when the box was the first volume.
	 */
	ContactInfo SphereBoxContact(const ColliderVolume& sphere, const ColliderVolume& box)
	{
		ContactInfo contact = {};

		const Vector3 delta = Vector3Subtract(sphere.center, box.center);
		const float extents[3] = { box.halfExtents.x, box.halfExtents.y, box.halfExtents.z };

		// Sphere centre expressed along the box's own axes, then clamped into the
		// box. That clamp is the closest point, which is the whole test.
		float local[3] = {};
		Vector3 closest = box.center;
		for (int i = 0; i < 3; ++i)
		{
			local[i] = Vector3DotProduct(delta, box.axes[i]);
			const float clamped = Clamp(local[i], -extents[i], extents[i]);
			closest = Vector3Add(closest, Vector3Scale(box.axes[i], clamped));
		}

		const Vector3 toSphere = Vector3Subtract(sphere.center, closest);
		const float distanceSquared = Vector3LengthSqr(toSphere);

		if (distanceSquared >= sphere.radius * sphere.radius) { return contact; }

		if (distanceSquared < kAxisEpsilon)
		{
			// The centre is inside the box, so the closest surface point is the
			// centre itself and there is no direction to escape along. Leave
			// through the nearest face — any other choice shoves the sphere
			// further in or across the box.
			int nearestAxis = 0;
			float leastPenetration = FLT_MAX;
			float sign = 1.0f;
			for (int i = 0; i < 3; ++i)
			{
				const float penetration = extents[i] - fabsf(local[i]);
				if (penetration < leastPenetration)
				{
					leastPenetration = penetration;
					nearestAxis = i;
					sign = (local[i] < 0.0f) ? -1.0f : 1.0f;
				}
			}

			contact.hit = true;
			contact.depth = leastPenetration + sphere.radius;
			// The escape direction is outward along that face, so the box — the
			// thing the sphere has to be pushed away from — lies the other way.
			contact.normal = Vector3Scale(box.axes[nearestAxis], -sign);
			return contact;
		}

		const float distance = sqrtf(distanceSquared);

		contact.hit = true;
		contact.depth = sphere.radius - distance;
		// toSphere runs from the box surface out to the sphere centre; the box is
		// the opposite way.
		contact.normal = Vector3Scale(toSphere, -1.0f / distance);
		return contact;
	}
}

#pragma region Volume
Quaternion SafeOrientation(Quaternion rotation)
{
	const float lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y
		+ rotation.z * rotation.z + rotation.w * rotation.w;
	if (lengthSquared < 1.0e-8f) { return QuaternionIdentity(); }
	return QuaternionNormalize(rotation);
}

ColliderVolume MakeColliderVolume(const Collider3D& collider, Vector3 translation,
	Quaternion rotation, Vector3 scale)
{
	const Quaternion orientation = SafeOrientation(rotation);

	ColliderVolume volume = {};
	volume.shape = collider.shape;

	// Rotation preserves length, so these come out unit length — which the
	// projection maths in the narrow phase relies on. Written through
	// Vector3RotateByQuaternion rather than a matrix so it cannot disagree with
	// the QuaternionToMatrix path render3D draws through, and so it does not
	// depend on raylib's matrix storage order.
	volume.axes[0] = Vector3RotateByQuaternion(Vector3{ 1.0f, 0.0f, 0.0f }, orientation);
	volume.axes[1] = Vector3RotateByQuaternion(Vector3{ 0.0f, 1.0f, 0.0f }, orientation);
	volume.axes[2] = Vector3RotateByQuaternion(Vector3{ 0.0f, 0.0f, 1.0f }, orientation);

	// Scale first, then rotate, then translate — the same order render3D bakes
	// into model.transform, so a collider offset lands where the art does.
	volume.center = Vector3Add(translation,
		Vector3RotateByQuaternion(collider.GetLocalOffset(scale), orientation));

	volume.halfExtents = collider.GetLocalHalfExtents(scale);
	volume.radius = collider.GetWorldRadius(scale);
	return volume;
}

Vector3 ColliderWorldHalfExtents(const ColliderVolume& volume)
{
	if (volume.shape == COLLIDER_SPHERE)
	{
		return { volume.radius, volume.radius, volume.radius };
	}

	// Sum the absolute contribution of each rotated local axis to each world
	// axis: the standard |R| * halfExtents construction.
	const Vector3 half = volume.halfExtents;
	return {
		fabsf(volume.axes[0].x) * half.x + fabsf(volume.axes[1].x) * half.y + fabsf(volume.axes[2].x) * half.z,
		fabsf(volume.axes[0].y) * half.x + fabsf(volume.axes[1].y) * half.y + fabsf(volume.axes[2].y) * half.z,
		fabsf(volume.axes[0].z) * half.x + fabsf(volume.axes[1].z) * half.y + fabsf(volume.axes[2].z) * half.z
	};
}
#pragma endregion

#pragma region Contact
ContactInfo ColliderContact(const ColliderVolume& a, const ColliderVolume& b)
{
	const bool sphereA = (a.shape == COLLIDER_SPHERE);
	const bool sphereB = (b.shape == COLLIDER_SPHERE);

	if (sphereA && sphereB) { return SphereSphereContact(a, b); }
	if (sphereA) { return SphereBoxContact(a, b); }

	if (sphereB)
	{
		// Same test with the roles swapped, so the normal comes back pointing
		// from the sphere toward the box and has to be flipped to honour the
		// "from a toward b" contract every caller relies on.
		ContactInfo contact = SphereBoxContact(b, a);
		if (contact.hit) { contact.normal = Vector3Negate(contact.normal); }
		return contact;
	}

	// COLLIDER_MESH is an ordinary box once FitToModel has sized it
	return BoxBoxContact(a, b);
}
#pragma endregion

#pragma region Raycast
bool ColliderRaycast(const ColliderVolume& volume, Ray ray, float& outDistance, Vector3& outNormal)
{
	if (Vector3LengthSqr(ray.direction) < kParallelEpsilon) { return false; }
	const Vector3 direction = Vector3Normalize(ray.direction);

	if (volume.shape == COLLIDER_SPHERE)
	{
		// Ray/sphere with a unit direction, so the quadratic's leading coefficient
		// is 1 and it reduces to a projection and a chord half-length.
		const Vector3 toCenter = Vector3Subtract(volume.center, ray.position);
		const float alongRay = Vector3DotProduct(toCenter, direction);
		const float radiusSquared = volume.radius * volume.radius;

		// Squared distance from the sphere's centre to the ray's infinite line
		const float missDistanceSquared = Vector3LengthSqr(toCenter) - alongRay * alongRay;
		if (missDistanceSquared > radiusSquared) { return false; }

		const float halfChord = sqrtf(fmaxf(radiusSquared - missDistanceSquared, 0.0f));
		const float entry = alongRay - halfChord;
		const float exit = alongRay + halfChord;

		if (exit < 0.0f) { return false; } // the whole sphere is behind the ray

		// Starting inside returns the exit, matching the box path above so a
		// listener or camera inside a volume behaves the same for either shape.
		const bool inside = entry < 0.0f;
		outDistance = inside ? exit : entry;

		const Vector3 surface = Vector3Add(ray.position, Vector3Scale(direction, outDistance));
		const Vector3 outward = Vector3Subtract(surface, volume.center);
		const float length = Vector3Length(outward);

		// A collapsed sphere has no surface to take a normal from; face the ray.
		Vector3 normal = (length > kParallelEpsilon)
			? Vector3Scale(outward, 1.0f / length)
			: Vector3Negate(direction);

		// Same convention as the slab test: face the ray from either side.
		outNormal = inside ? Vector3Negate(normal) : normal;
		return true;
	}

	// Box and mesh. Push the ray into the volume's own frame and slab-test there.
	// ColliderVolume already carries unit axes, so this is three dot products
	// rather than a quaternion inverse.
	const Vector3 delta = Vector3Subtract(ray.position, volume.center);
	const Vector3 localOrigin = {
		Vector3DotProduct(delta, volume.axes[0]),
		Vector3DotProduct(delta, volume.axes[1]),
		Vector3DotProduct(delta, volume.axes[2])
	};
	const Vector3 localDirection = {
		Vector3DotProduct(direction, volume.axes[0]),
		Vector3DotProduct(direction, volume.axes[1]),
		Vector3DotProduct(direction, volume.axes[2])
	};

	Vector3 localNormal = {};
	if (!SlabTest(localOrigin, localDirection, volume.halfExtents, outDistance, localNormal))
	{
		return false;
	}

	// Back out to world space along the same axes
	outNormal = Vector3Add(Vector3Add(
		Vector3Scale(volume.axes[0], localNormal.x),
		Vector3Scale(volume.axes[1], localNormal.y)),
		Vector3Scale(volume.axes[2], localNormal.z));
	return true;
}
#pragma endregion

#pragma region Extents
Vector3 SanitizeColliderSize(Vector3 size)
{
	return {
		fmaxf(size.x, MINIMUM_COLLIDER_EXTENT),
		fmaxf(size.y, MINIMUM_COLLIDER_EXTENT),
		fmaxf(size.z, MINIMUM_COLLIDER_EXTENT)
	};
}

Vector3 Collider3D::GetLocalHalfExtents(Vector3 bodyScale) const
{
	// size * bodyScale * 0.5 — with the default size of {1,1,1} this is exactly
	// `bodyScale * 0.5f`, the expression the body used before colliders existed.
	// That identity is what keeps every pre-existing save behaving the same.
	const Vector3 extent = Vector3Multiply(size, bodyScale);
	return {
		RepairExtent(extent.x) * 0.5f,
		RepairExtent(extent.y) * 0.5f,
		RepairExtent(extent.z) * 0.5f
	};
}

Vector3 Collider3D::GetLocalOffset(Vector3 bodyScale) const
{
	// Scaled but not rotated: the body applies its own rotation, exactly as
	// render3D scales a mesh before rotating it.
	return Vector3Multiply(offset, bodyScale);
}

float Collider3D::GetWorldRadius(Vector3 bodyScale) const
{
	const float largest = fmaxf(fabsf(bodyScale.x), fmaxf(fabsf(bodyScale.y), fabsf(bodyScale.z)));
	return fmaxf(radius * largest, MINIMUM_COLLIDER_EXTENT);
}
#pragma endregion

#pragma region Mesh Fitting
void Collider3D::FitToBounds(BoundingBox bounds)
{
	size = SanitizeColliderSize(Vector3Subtract(bounds.max, bounds.min));
	offset = Vector3Scale(Vector3Add(bounds.max, bounds.min), 0.5f);

	// A sphere collider fitted to the same bounds should still enclose them, so
	// take the largest half extent rather than the smallest.
	const Vector3 half = Vector3Scale(size, 0.5f);
	radius = fmaxf(half.x, fmaxf(half.y, half.z));
}

void Collider3D::FitToModel(const Model& model)
{
	if (model.meshCount <= 0 || model.meshes == nullptr) { return; }

	// model.transform is deliberately ignored: GameObject::render3D overwrites it
	// every frame with the body's own scale and rotation, so the mesh's untouched
	// local bounds are what that transform is applied to — and local units are
	// precisely the frame `size` and `offset` live in.
	BoundingBox bounds = {};
	bool haveBounds = false;

	for (int i = 0; i < model.meshCount; ++i)
	{
		const Mesh& mesh = model.meshes[i];
		if (mesh.vertexCount <= 0 || mesh.vertices == nullptr) { continue; }

		const BoundingBox meshBounds = GetMeshBoundingBox(mesh);
		if (!haveBounds)
		{
			bounds = meshBounds;
			haveBounds = true;
			continue;
		}

		bounds.min = Vector3Min(bounds.min, meshBounds.min);
		bounds.max = Vector3Max(bounds.max, meshBounds.max);
	}

	// Nothing to measure. Leaving the collider as it was beats collapsing it to
	// a point, which would make the object fall through the world.
	if (!haveBounds) { return; }

	FitToBounds(bounds);
}
#pragma endregion

#pragma region SaveData
Json Collider3D::formatToJson() const
{
	Json j;

	j["Shape"] = static_cast<int>(shape);
	j["Mode"] = static_cast<int>(mode);

	j["SizeX"] = size.x;
	j["SizeY"] = size.y;
	j["SizeZ"] = size.z;

	j["OffsetX"] = offset.x;
	j["OffsetY"] = offset.y;
	j["OffsetZ"] = offset.z;

	j["Radius"] = radius;

	return j;
}

bool Collider3D::loadFromJson(const Json& j)
{
	*this = {};

	if (!j.is_object()) { return false; }

	// Out-of-range values fall back to the default rather than being cast into a
	// shape or mode that does not exist — a save from a future build that added
	// a shape must not index off the end of the dispatch.
	const int loadedShape = j.value("Shape", static_cast<int>(COLLIDER_BOX));
	const int loadedMode = j.value("Mode", static_cast<int>(COLLIDER_COLLISION));

	shape = (loadedShape >= 0 && loadedShape < COLLIDER_SHAPE_COUNT)
		? static_cast<ColliderShape>(loadedShape)
		: COLLIDER_BOX;
	mode = (loadedMode >= 0 && loadedMode < COLLIDER_MODE_COUNT)
		? static_cast<ColliderMode>(loadedMode)
		: COLLIDER_COLLISION;

	size = SanitizeColliderSize({
		j.value("SizeX", 1.0f),
		j.value("SizeY", 1.0f),
		j.value("SizeZ", 1.0f) });

	offset = {
		j.value("OffsetX", 0.0f),
		j.value("OffsetY", 0.0f),
		j.value("OffsetZ", 0.0f) };

	radius = fmaxf(j.value("Radius", 0.5f), MINIMUM_COLLIDER_EXTENT);

	return true;
}
#pragma endregion
