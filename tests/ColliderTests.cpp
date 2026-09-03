/**
 * Standalone tests for the 3D collider component.
 *
 * Deliberately headless. Meshes and models are built by hand rather than with
 * GenMeshCube, because GenMeshCube calls UploadMesh and needs a GL context —
 * which is why tests/StalkerFsmTests.cpp has to InitWindow and run under xvfb.
 * GetMeshBoundingBox only reads mesh.vertices and mesh.vertexCount, so avoiding
 * the generators keeps this suite in the cheap tier alongside SoundFieldTests:
 * raylib linked, nothing initialised, nothing drawn.
 *
 * That is only possible because Collider3D.cpp has no engine dependencies. If a
 * game header ever appears in it, this file stops linking.
 *
 * Build and run: tests/run_tests.sh
 */

#include <Collider.h>

#include <raymath.h>

#include <cmath>
#include <cstdio>
#include <string>

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

static void CheckNear(float actual, float expected, float tolerance, const std::string& what)
{
	++g_checks;
	if (std::fabs(actual - expected) > tolerance)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected ~%.4f, got %.4f)\n", what.c_str(), expected, actual);
	}
}

static void CheckVector(Vector3 actual, Vector3 expected, float tolerance, const std::string& what)
{
	++g_checks;
	if (std::fabs(actual.x - expected.x) > tolerance
		|| std::fabs(actual.y - expected.y) > tolerance
		|| std::fabs(actual.z - expected.z) > tolerance)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected ~[%.4f %.4f %.4f], got [%.4f %.4f %.4f])\n",
			what.c_str(), expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
	}
}

// ─── Fixtures ──────────────────────────────────────────────────────────────

static ColliderVolume BoxAt(Vector3 position, Vector3 scale = Vector3{ 1, 1, 1 },
	Quaternion rotation = Quaternion{ 0, 0, 0, 1 })
{
	Collider3D collider = {};
	return MakeColliderVolume(collider, position, rotation, scale);
}

static ColliderVolume SphereAt(Vector3 position, float radius, Vector3 scale = Vector3{ 1, 1, 1 })
{
	Collider3D collider = {};
	collider.shape = COLLIDER_SPHERE;
	collider.radius = radius;
	return MakeColliderVolume(collider, position, Quaternion{ 0, 0, 0, 1 }, scale);
}

// A mesh needs only vertexCount and vertices for GetMeshBoundingBox, which
// sweeps them for a min/max. Two opposite corners define a box.
static Mesh MeshFromCorners(float* corners)
{
	Mesh mesh = {};
	mesh.vertexCount = 2;
	mesh.vertices = corners;
	return mesh;
}

static Model ModelFromMeshes(Mesh* meshes, int count)
{
	Model model = {};
	model.meshCount = count;
	model.meshes = meshes;
	return model;
}

// ─── Tests ─────────────────────────────────────────────────────────────────

/**
 * The guarantee the whole design rests on: a default collider must reproduce
 * the numbers the body produced before colliders existed, where the collision
 * half extents were literally `scale * 0.5f`.
 */
static void TestDefaultReproducesLegacyExtents()
{
	std::printf("default collider reproduces the pre-collider extents\n");

	Collider3D collider = {};
	const Vector3 scale = { 2.0f, 3.0f, 4.0f };

	CheckVector(collider.GetLocalHalfExtents(scale), Vector3{ 1.0f, 1.5f, 2.0f }, 0.0001f,
		"local half extents are scale * 0.5");
	CheckVector(collider.GetLocalOffset(scale), Vector3{ 0, 0, 0 }, 0.0001f,
		"a default collider adds no offset");

	const ColliderVolume volume = MakeColliderVolume(collider, Vector3{ 5, 6, 7 },
		Quaternion{ 0, 0, 0, 1 }, scale);
	CheckVector(volume.center, Vector3{ 5, 6, 7 }, 0.0001f,
		"a default collider sits on the body's translation");

	// A quarter turn about Y swaps the X and Z extents, which is the |R| * half
	// construction the broad-phase box is built from.
	const Quaternion quarterTurn = QuaternionFromAxisAngle(Vector3{ 0, 1, 0 }, PI * 0.5f);
	const ColliderVolume rotated = MakeColliderVolume(collider, Vector3{ 0, 0, 0 }, quarterTurn, scale);
	CheckVector(ColliderWorldHalfExtents(rotated), Vector3{ 2.0f, 1.5f, 1.0f }, 0.0001f,
		"world half extents follow rotation");

	CheckVector(ColliderWorldHalfExtents(MakeColliderVolume(collider, Vector3{ 0, 0, 0 },
		Quaternion{ 0, 0, 0, 1 }, scale)), Vector3{ 1.0f, 1.5f, 2.0f }, 0.0001f,
		"an unrotated body's world half extents equal its local ones");
}

static void TestDegenerateRotationIsRepaired()
{
	std::printf("a zeroed quaternion does not collapse the volume\n");

	Collider3D collider = {};
	const ColliderVolume volume = MakeColliderVolume(collider, Vector3{ 0, 0, 0 },
		Quaternion{ 0, 0, 0, 0 }, Vector3{ 1, 1, 1 });

	CheckVector(volume.axes[0], Vector3{ 1, 0, 0 }, 0.0001f, "x axis survives a zeroed rotation");
	CheckVector(volume.axes[1], Vector3{ 0, 1, 0 }, 0.0001f, "y axis survives a zeroed rotation");
	CheckVector(volume.axes[2], Vector3{ 0, 0, 1 }, 0.0001f, "z axis survives a zeroed rotation");
}

static void TestDegenerateSizeIsRepaired()
{
	std::printf("a zero or negative size never inverts the volume\n");

	Collider3D collider = {};
	collider.size = { 0.0f, -2.0f, 1.0f };

	const Vector3 half = collider.GetLocalHalfExtents(Vector3{ 1, 1, 1 });
	Check(half.x > 0.0f && half.y > 0.0f && half.z > 0.0f,
		"every half extent stays positive");
	CheckNear(half.z, 0.5f, 0.0001f, "a valid component is not disturbed by repairing its neighbours");

	CheckVector(SanitizeColliderSize(Vector3{ -1.0f, 0.0f, 3.0f }),
		Vector3{ MINIMUM_COLLIDER_EXTENT, MINIMUM_COLLIDER_EXTENT, 3.0f }, 0.0001f,
		"SanitizeColliderSize clamps only what is below the minimum");
}

static void TestBoxSeparationAndOverlap()
{
	std::printf("box against box\n");

	const ColliderVolume a = BoxAt(Vector3{ 0, 0, 0 });
	Check(!ColliderContact(a, BoxAt(Vector3{ 2.0f, 0, 0 })).hit, "boxes two apart do not touch");
	Check(!ColliderContact(a, BoxAt(Vector3{ 1.0f, 0, 0 })).hit,
		"boxes exactly touching count as apart");

	const ContactInfo contact = ColliderContact(a, BoxAt(Vector3{ 0.5f, 0, 0 }));
	Check(contact.hit, "overlapping boxes report a contact");
	CheckNear(contact.depth, 0.5f, 0.0001f, "penetration depth is the overlap on the winning axis");
	CheckVector(contact.normal, Vector3{ 1, 0, 0 }, 0.0001f,
		"the normal points from the first box toward the second");

	const ContactInfo mirrored = ColliderContact(BoxAt(Vector3{ 0.5f, 0, 0 }), a);
	CheckVector(mirrored.normal, Vector3{ -1, 0, 0 }, 0.0001f,
		"swapping the arguments flips the normal");
}

/**
 * The case an axis-aligned solver cannot see.
 *
 * Two boxes can be genuinely apart while every one of the six face axes reports
 * an overlap — only a cross product of one edge from each separates them. This
 * asserts it directly by running a face-axes-only test alongside the real one:
 * if the nine cross products are ever dropped from ColliderContact, the two
 * disagree and this fails.
 */
static void TestEdgeEdgeSeparation()
{
	std::printf("edge-edge separation (the axis-aligned blind spot)\n");

	const Quaternion tilt = { 0.01675516f, 0.77474071f, -0.54374293f, 0.32224172f };
	const Vector3 offset = { -0.71957869f, 0.33132638f, 1.16404325f };

	const ColliderVolume a = BoxAt(Vector3{ 0, 0, 0 });
	const ColliderVolume b = BoxAt(offset, Vector3{ 1, 1, 1 }, tilt);

	// Face axes alone: the six normals of the two boxes, nothing else.
	bool faceAxesSaySeparated = false;
	for (int i = 0; i < 6; ++i)
	{
		const Vector3 axis = (i < 3) ? a.axes[i] : b.axes[i - 3];
		const Vector3 delta = Vector3Subtract(b.center, a.center);

		const auto projectedRadius = [](const ColliderVolume& volume, Vector3 onto) {
			return std::fabs(Vector3DotProduct(onto, volume.axes[0])) * volume.halfExtents.x
				+ std::fabs(Vector3DotProduct(onto, volume.axes[1])) * volume.halfExtents.y
				+ std::fabs(Vector3DotProduct(onto, volume.axes[2])) * volume.halfExtents.z;
		};

		const float overlap = projectedRadius(a, axis) + projectedRadius(b, axis)
			- std::fabs(Vector3DotProduct(delta, axis));
		if (overlap <= 0.0f) { faceAxesSaySeparated = true; break; }
	}

	Check(!faceAxesSaySeparated, "the fixture is one the face axes alone call overlapping");
	Check(!ColliderContact(a, b).hit,
		"the nine edge-edge axes prove the boxes are actually apart");
}

static void TestRestingBoxNormalIsVertical()
{
	std::printf("a box resting on a floor is pushed up, not sideways\n");

	// A shallow overlap on a wide floor puts several axes within noise of each
	// other. Picking an edge axis here would shove the box sideways — the
	// resting jitter the face-axis preference exists to prevent.
	const ColliderVolume box = BoxAt(Vector3{ 0.0f, 0.49f, 0.0f });
	const ColliderVolume floor = BoxAt(Vector3{ 0.0f, -0.5f, 0.0f }, Vector3{ 10.0f, 1.0f, 10.0f });

	const ContactInfo contact = ColliderContact(box, floor);
	Check(contact.hit, "the resting box is in contact with the floor");
	CheckVector(contact.normal, Vector3{ 0, -1, 0 }, 0.0001f,
		"the normal points straight down, from the box toward the floor");
	CheckNear(contact.depth, 0.01f, 0.0001f, "depth is the shallow vertical overlap");
}

static void TestSphereAgainstSphere()
{
	std::printf("sphere against sphere\n");

	const ColliderVolume a = SphereAt(Vector3{ 0, 0, 0 }, 0.5f);

	Check(!ColliderContact(a, SphereAt(Vector3{ 2.0f, 0, 0 }, 0.5f)).hit, "distant spheres do not touch");
	Check(!ColliderContact(a, SphereAt(Vector3{ 1.0f, 0, 0 }, 0.5f)).hit,
		"spheres exactly touching count as apart");

	const ContactInfo contact = ColliderContact(a, SphereAt(Vector3{ 0.5f, 0, 0 }, 0.5f));
	Check(contact.hit, "overlapping spheres report a contact");
	CheckNear(contact.depth, 0.5f, 0.0001f, "depth is the sum of radii minus the distance");
	CheckVector(contact.normal, Vector3{ 1, 0, 0 }, 0.0001f, "the normal runs centre to centre");

	// Concentric spheres have no separating direction; the fallback must still
	// hand the solver a finite unit vector rather than a NaN.
	const ContactInfo concentric = ColliderContact(a, SphereAt(Vector3{ 0, 0, 0 }, 0.5f));
	Check(concentric.hit, "concentric spheres report a contact");
	CheckNear(Vector3Length(concentric.normal), 1.0f, 0.0001f,
		"concentric spheres still produce a unit normal");
}

static void TestSphereRadiusFollowsScale()
{
	std::printf("sphere radius follows the body scale\n");

	Collider3D collider = {};
	collider.shape = COLLIDER_SPHERE;
	collider.radius = 0.5f;

	CheckNear(collider.GetWorldRadius(Vector3{ 2, 2, 2 }), 1.0f, 0.0001f,
		"uniform scale multiplies the radius");
	// A non-uniform scale would make an ellipsoid, which this solver has no
	// shape for. Taking the largest component over-covers rather than under.
	CheckNear(collider.GetWorldRadius(Vector3{ 1, 4, 2 }), 2.0f, 0.0001f,
		"non-uniform scale takes the largest component");
}

static void TestSphereAgainstBox()
{
	std::printf("sphere against box\n");

	const ColliderVolume box = BoxAt(Vector3{ 0, 0, 0 });

	// Face contact
	const ContactInfo face = ColliderContact(SphereAt(Vector3{ 0.8f, 0, 0 }, 0.5f), box);
	Check(face.hit, "a sphere overlapping a face reports a contact");
	CheckNear(face.depth, 0.2f, 0.0001f, "depth is the radius minus the distance to the face");
	CheckVector(face.normal, Vector3{ -1, 0, 0 }, 0.0001f,
		"the normal points from the sphere toward the box");

	// Argument order must not change the answer, only the normal's sign
	const ContactInfo flipped = ColliderContact(box, SphereAt(Vector3{ 0.8f, 0, 0 }, 0.5f));
	Check(flipped.hit, "the same pair reported box-first still touches");
	CheckNear(flipped.depth, 0.2f, 0.0001f, "depth is independent of argument order");
	CheckVector(flipped.normal, Vector3{ 1, 0, 0 }, 0.0001f, "box-first flips the normal");

	// Diagonal miss: the sphere's own bounding box overlaps the box, but the
	// sphere itself clears the corner. An AABB test gets this wrong.
	Check(!ColliderContact(SphereAt(Vector3{ 0.8f, 0.8f, 0.8f }, 0.5f), box).hit,
		"a sphere clearing a corner does not touch, though its AABB overlaps");

	const ContactInfo corner = ColliderContact(SphereAt(Vector3{ 0.7f, 0.7f, 0.7f }, 0.5f), box);
	Check(corner.hit, "a sphere overlapping a corner reports a contact");
	CheckNear(corner.depth, 0.5f - std::sqrt(3.0f) * 0.2f, 0.0001f,
		"corner depth is the radius minus the distance to the corner");

	// Centre inside: there is no closest surface point to aim at, so the sphere
	// has to leave through the nearest face.
	const ColliderVolume bigBox = BoxAt(Vector3{ 0, 0, 0 }, Vector3{ 2, 2, 2 });
	const ContactInfo inside = ColliderContact(SphereAt(Vector3{ 0.6f, 0, 0 }, 0.5f), bigBox);
	Check(inside.hit, "a sphere centred inside a box reports a contact");
	CheckVector(inside.normal, Vector3{ -1, 0, 0 }, 0.0001f,
		"an engulfed sphere is pushed out through the nearest face");
	CheckNear(inside.depth, 0.9f, 0.0001f,
		"depth carries the sphere fully clear of that face");
}

static void TestSizeShrinksTheVolume()
{
	std::printf("collider size is independent of render scale\n");

	Collider3D full = {};
	Collider3D half = {};
	half.size = { 0.5f, 0.5f, 0.5f };

	const Vector3 farApart = { 0.6f, 0, 0 };
	const Quaternion identity = { 0, 0, 0, 1 };
	const Vector3 unit = { 1, 1, 1 };

	Check(ColliderContact(MakeColliderVolume(full, Vector3{ 0, 0, 0 }, identity, unit),
		MakeColliderVolume(full, farApart, identity, unit)).hit,
		"full-size colliders at 0.6 apart overlap");

	Check(!ColliderContact(MakeColliderVolume(half, Vector3{ 0, 0, 0 }, identity, unit),
		MakeColliderVolume(half, farApart, identity, unit)).hit,
		"half-size colliders on the same bodies do not");
}

static void TestOffsetMovesTheVolume()
{
	std::printf("collider offset moves the volume with the body\n");

	Collider3D collider = {};
	collider.offset = { 1.0f, 0.0f, 0.0f };

	CheckVector(MakeColliderVolume(collider, Vector3{ 0, 0, 0 }, Quaternion{ 0, 0, 0, 1 },
		Vector3{ 1, 1, 1 }).center, Vector3{ 1, 0, 0 }, 0.0001f,
		"the offset shifts the centre off the translation");

	CheckVector(MakeColliderVolume(collider, Vector3{ 0, 0, 0 }, Quaternion{ 0, 0, 0, 1 },
		Vector3{ 2, 1, 1 }).center, Vector3{ 2, 0, 0 }, 0.0001f,
		"the offset is scaled with the body");

	// A quarter turn about Y takes +X to -Z, so an offset collider swings round
	// with the art rather than staying put in world space.
	const Quaternion quarterTurn = QuaternionFromAxisAngle(Vector3{ 0, 1, 0 }, PI * 0.5f);
	CheckVector(MakeColliderVolume(collider, Vector3{ 0, 0, 0 }, quarterTurn,
		Vector3{ 1, 1, 1 }).center, Vector3{ 0, 0, -1 }, 0.0001f,
		"the offset is rotated with the body");

	// The offset has to reach the contact, not just the reported centre
	Collider3D plain = {};
	Check(!ColliderContact(MakeColliderVolume(plain, Vector3{ 0, 0, 0 }, Quaternion{ 0, 0, 0, 1 }, Vector3{ 1, 1, 1 }),
		MakeColliderVolume(plain, Vector3{ 1.5f, 0, 0 }, Quaternion{ 0, 0, 0, 1 }, Vector3{ 1, 1, 1 })).hit,
		"two unoffset boxes 1.5 apart do not touch");
	Check(ColliderContact(MakeColliderVolume(collider, Vector3{ 0, 0, 0 }, Quaternion{ 0, 0, 0, 1 }, Vector3{ 1, 1, 1 }),
		MakeColliderVolume(plain, Vector3{ 1.5f, 0, 0 }, Quaternion{ 0, 0, 0, 1 }, Vector3{ 1, 1, 1 })).hit,
		"offsetting one of them toward the other makes them touch");
}

static void TestMeshFitting()
{
	std::printf("mesh fitting\n");

	// A unit cube is the compatibility case: fitting one must leave a collider
	// indistinguishable from the default, because that is what every object in
	// the game currently renders.
	float unitCube[6] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
	Mesh unitMesh = MeshFromCorners(unitCube);
	Model unitModel = ModelFromMeshes(&unitMesh, 1);

	Collider3D collider = {};
	collider.shape = COLLIDER_MESH;
	collider.FitToModel(unitModel);
	CheckVector(collider.size, Vector3{ 1, 1, 1 }, 0.0001f, "a unit cube fits the default size");
	CheckVector(collider.offset, Vector3{ 0, 0, 0 }, 0.0001f, "a centred mesh needs no offset");

	// A non-cube model
	float oblong[6] = { -1.0f, -1.5f, -2.0f, 1.0f, 1.5f, 2.0f };
	Mesh oblongMesh = MeshFromCorners(oblong);
	Model oblongModel = ModelFromMeshes(&oblongMesh, 1);
	collider.FitToModel(oblongModel);
	CheckVector(collider.size, Vector3{ 2, 3, 4 }, 0.0001f, "size is the mesh's own extent");
	CheckVector(collider.offset, Vector3{ 0, 0, 0 }, 0.0001f, "a centred oblong needs no offset");
	CheckNear(collider.radius, 2.0f, 0.0001f, "a fitted sphere radius still encloses the mesh");

	// Art is rarely modelled around its origin
	float offCentre[6] = { 0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f };
	Mesh offCentreMesh = MeshFromCorners(offCentre);
	Model offCentreModel = ModelFromMeshes(&offCentreMesh, 1);
	collider.FitToModel(offCentreModel);
	CheckVector(collider.size, Vector3{ 2, 2, 2 }, 0.0001f, "an off-centre mesh keeps its extent");
	CheckVector(collider.offset, Vector3{ 1, 1, 1 }, 0.0001f,
		"the offset recentres the collider on the geometry");

	// Multi-mesh models must union, not take the first mesh
	float lower[6] = { -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f };
	float upper[6] = { 0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f };
	Mesh parts[2] = { MeshFromCorners(lower), MeshFromCorners(upper) };
	Model multiModel = ModelFromMeshes(parts, 2);
	collider.FitToModel(multiModel);
	CheckVector(collider.size, Vector3{ 3, 3, 3 }, 0.0001f, "every mesh in the model is unioned");
	CheckVector(collider.offset, Vector3{ 0.5f, 0.5f, 0.5f }, 0.0001f, "the union is recentred");

	// An empty model must leave the collider alone. Collapsing it to a point
	// would drop the object through the world.
	Model emptyModel = ModelFromMeshes(nullptr, 0);
	const Vector3 sizeBefore = collider.size;
	collider.FitToModel(emptyModel);
	CheckVector(collider.size, sizeBefore, 0.0001f, "a model with no meshes leaves the collider alone");
}

/**
 * The reason ColliderRaycast exists at all.
 *
 * Every ray test in the game used to run against the axis-aligned box enclosing
 * a body. A rotated box sits some way inside its own bounds, and a ray through
 * that gap was a miss reported as a hit — which is how a player ends up grounded
 * on thin air beside a 45-degree wall.
 */
static void TestRayMissesRotatedBoxInsideItsBounds()
{
	std::printf("a ray through a rotated box's bounds but not the box\n");

	// A unit box turned 45 degrees about Y. Its enclosing AABB grows to
	// sqrt(2)/2 ~ 0.707 on X and Z, and the gap between the two is at the AABB's
	// CORNERS — the rotated square still reaches the full 0.707 along each axis
	// on its own, just at a single corner each. So the ray has to go up the
	// diagonal: (0.6, 0.6) is comfortably inside the bounding box and comfortably
	// outside the box it encloses.
	const Quaternion turned = QuaternionFromAxisAngle(Vector3{ 0, 1, 0 }, PI * 0.25f);
	const ColliderVolume rotated = BoxAt(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, turned);

	const Vector3 aabbHalf = ColliderWorldHalfExtents(rotated);
	Check(aabbHalf.x > 0.6f && aabbHalf.z > 0.6f,
		"the fixture's bounding box really does contain the ray");

	Ray through = {};
	through.position = Vector3{ 0.6f, -5.0f, 0.6f };
	through.direction = Vector3{ 0, 1, 0 };

	float distance = 0.0f;
	Vector3 normal = {};
	Check(!ColliderRaycast(rotated, through, distance, normal),
		"the ray misses the rotated box it passes beside");

	// And the same ray does hit once the box is not rotated, so the fixture is
	// measuring rotation rather than simply being aimed at nothing.
	const ColliderVolume square = BoxAt(Vector3{ 0, 0, 0 }, Vector3{ 1.4f, 1, 1.4f });
	Check(ColliderRaycast(square, through, distance, normal),
		"the same ray hits a box that actually fills those bounds");
}

static void TestRayAgainstBox()
{
	std::printf("ray against box\n");

	const ColliderVolume box = BoxAt(Vector3{ 0, 0, 0 });

	float distance = 0.0f;
	Vector3 normal = {};

	Ray ray = {};
	ray.position = Vector3{ -3.0f, 0.0f, 0.0f };
	ray.direction = Vector3{ 1, 0, 0 };
	Check(ColliderRaycast(box, ray, distance, normal), "a ray aimed at the box hits it");
	CheckNear(distance, 2.5f, 0.0001f, "distance is to the near face, not the centre");
	CheckVector(normal, Vector3{ -1, 0, 0 }, 0.0001f, "the normal is the entry face, facing the ray");

	// Behind the ray
	ray.direction = Vector3{ -1, 0, 0 };
	Check(!ColliderRaycast(box, ray, distance, normal), "a box behind the ray is not hit");

	// Aimed past it
	ray.position = Vector3{ -3.0f, 2.0f, 0.0f };
	ray.direction = Vector3{ 1, 0, 0 };
	Check(!ColliderRaycast(box, ray, distance, normal), "a ray passing above the box misses");

	// A zero-length direction must be rejected rather than normalised
	ray.position = Vector3{ 0, 0, 0 };
	ray.direction = Vector3{ 0, 0, 0 };
	Check(!ColliderRaycast(box, ray, distance, normal), "a zero-length direction is rejected");
}

/**
 * Starting inside must report the exit, not zero. An editor camera flown inside
 * a room would otherwise have every click swallowed by the walls around it, and
 * a sound emitted inside a volume would occlude itself.
 */
static void TestRayStartingInsideReportsTheExit()
{
	std::printf("a ray starting inside a volume reports its exit\n");

	Ray outward = {};
	outward.position = Vector3{ 0, 0, 0 };
	outward.direction = Vector3{ 1, 0, 0 };

	float distance = 0.0f;
	Vector3 normal = {};

	const ColliderVolume box = BoxAt(Vector3{ 0, 0, 0 }, Vector3{ 4, 4, 4 });
	Check(ColliderRaycast(box, outward, distance, normal), "a ray inside a box still reports a hit");
	CheckNear(distance, 2.0f, 0.0001f, "the distance is to the far face");
	CheckVector(normal, Vector3{ -1, 0, 0 }, 0.0001f, "the exit normal faces back down the ray");

	const ColliderVolume sphere = SphereAt(Vector3{ 0, 0, 0 }, 2.0f);
	Check(ColliderRaycast(sphere, outward, distance, normal), "a ray inside a sphere still reports a hit");
	CheckNear(distance, 2.0f, 0.0001f, "the distance is to the far surface");
	CheckVector(normal, Vector3{ -1, 0, 0 }, 0.0001f, "the sphere agrees with the box on inside normals");
}

static void TestRayAgainstSphere()
{
	std::printf("ray against sphere\n");

	const ColliderVolume sphere = SphereAt(Vector3{ 0, 0, 0 }, 0.5f);

	Ray ray = {};
	ray.position = Vector3{ -3.0f, 0.0f, 0.0f };
	ray.direction = Vector3{ 1, 0, 0 };

	float distance = 0.0f;
	Vector3 normal = {};
	Check(ColliderRaycast(sphere, ray, distance, normal), "a ray through the centre hits");
	CheckNear(distance, 2.5f, 0.0001f, "distance is to the near surface");
	CheckVector(normal, Vector3{ -1, 0, 0 }, 0.0001f, "the normal faces the ray");

	// The discriminating case: a ray that clips the sphere's bounding box but
	// misses the sphere. Testing the enclosing box would call this a hit.
	ray.position = Vector3{ -3.0f, 0.45f, 0.45f };
	Check(!ColliderRaycast(sphere, ray, distance, normal),
		"a ray through the bounding box's corner misses the sphere inside it");

	ray.position = Vector3{ -3.0f, 2.0f, 0.0f };
	Check(!ColliderRaycast(sphere, ray, distance, normal), "a ray passing well clear misses");
}

/**
 * A ray has to be tested where the collider is, not where the object's origin
 * is. This is what makes an offset collider usable at all.
 */
static void TestRayFollowsColliderOffset()
{
	std::printf("a ray hits an offset collider where the collider is\n");

	Collider3D offset = {};
	offset.offset = Vector3{ 0.0f, 5.0f, 0.0f };
	const ColliderVolume volume = MakeColliderVolume(offset, Vector3{ 0, 0, 0 },
		Quaternion{ 0, 0, 0, 1 }, Vector3{ 1, 1, 1 });

	Ray atTheOrigin = {};
	atTheOrigin.position = Vector3{ -3.0f, 0.0f, 0.0f };
	atTheOrigin.direction = Vector3{ 1, 0, 0 };

	Ray atTheCollider = atTheOrigin;
	atTheCollider.position.y = 5.0f;

	float distance = 0.0f;
	Vector3 normal = {};
	Check(!ColliderRaycast(volume, atTheOrigin, distance, normal),
		"nothing is hit where the transform sits");
	Check(ColliderRaycast(volume, atTheCollider, distance, normal),
		"the collider is hit where the offset put it");
}

/**
 * Mode must gate the RESPONSE, never the DETECTION. If a trigger ever stopped
 * reporting contacts here, every trigger volume in the game would go silent.
 */
static void TestTriggerStillDetects()
{
	std::printf("trigger mode does not suppress detection\n");

	Collider3D trigger = {};
	trigger.mode = COLLIDER_TRIGGER;
	Check(trigger.isTrigger(), "a trigger collider reports itself as one");

	Collider3D solid = {};
	Check(!solid.isTrigger(), "a collision collider does not");

	const Quaternion identity = { 0, 0, 0, 1 };
	const Vector3 unit = { 1, 1, 1 };
	Check(ColliderContact(MakeColliderVolume(trigger, Vector3{ 0, 0, 0 }, identity, unit),
		MakeColliderVolume(solid, Vector3{ 0.5f, 0, 0 }, identity, unit)).hit,
		"a trigger overlapping a solid still reports the contact");
}

static void TestJsonRoundTrip()
{
	std::printf("save and load\n");

	Collider3D saved = {};
	saved.shape = COLLIDER_SPHERE;
	saved.mode = COLLIDER_TRIGGER;
	saved.size = { 2.0f, 3.0f, 4.0f };
	saved.offset = { -1.0f, 0.5f, 2.0f };
	saved.radius = 1.25f;

	Collider3D loaded = {};
	Check(loaded.loadFromJson(saved.formatToJson()), "a written collider loads back");
	Check(loaded.shape == COLLIDER_SPHERE, "shape survives the round trip");
	Check(loaded.mode == COLLIDER_TRIGGER, "mode survives the round trip");
	CheckVector(loaded.size, saved.size, 0.0001f, "size survives the round trip");
	CheckVector(loaded.offset, saved.offset, 0.0001f, "offset survives the round trip");
	CheckNear(loaded.radius, saved.radius, 0.0001f, "radius survives the round trip");

	// A save written before colliders existed has no fields at all, and must
	// come back as the default box that reproduces the old behaviour.
	Collider3D legacy = {};
	legacy.shape = COLLIDER_SPHERE;
	Check(legacy.loadFromJson(Json::object()), "an empty object loads");
	Check(legacy.shape == COLLIDER_BOX, "a collider with no stored shape defaults to a box");
	Check(legacy.mode == COLLIDER_COLLISION, "and to collision mode");
	CheckVector(legacy.size, Vector3{ 1, 1, 1 }, 0.0001f, "and to the identity size");

	// A shape index from a future build must not index off the dispatch
	Json future = Json::object();
	future["Shape"] = 99;
	future["Mode"] = -3;
	Collider3D guarded = {};
	Check(guarded.loadFromJson(future), "an out-of-range shape still loads");
	Check(guarded.shape == COLLIDER_BOX, "an unknown shape falls back to a box");
	Check(guarded.mode == COLLIDER_COLLISION, "an unknown mode falls back to collision");

	Check(!guarded.loadFromJson(Json(5)), "a non-object is rejected");
}

int main()
{
	std::printf("Collider tests\n\n");

	TestDefaultReproducesLegacyExtents();
	TestDegenerateRotationIsRepaired();
	TestDegenerateSizeIsRepaired();
	TestBoxSeparationAndOverlap();
	TestEdgeEdgeSeparation();
	TestRestingBoxNormalIsVertical();
	TestSphereAgainstSphere();
	TestSphereRadiusFollowsScale();
	TestSphereAgainstBox();
	TestSizeShrinksTheVolume();
	TestOffsetMovesTheVolume();
	TestMeshFitting();
	TestRayAgainstBox();
	TestRayMissesRotatedBoxInsideItsBounds();
	TestRayStartingInsideReportsTheExit();
	TestRayAgainstSphere();
	TestRayFollowsColliderOffset();
	TestTriggerStillDetects();
	TestJsonRoundTrip();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
