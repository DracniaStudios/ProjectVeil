/**
 * Standalone tests for Geometry.h (work order B-03).
 *
 * Each helper in Geometry:: is a port of a raylib function, so the tests compare
 * it with raylib's own function on the same inputs. Booleans, Colors and bounding
 * boxes must be exactly equal (bitwise for floats); rays are compared to 1e-5 and
 * the suite also reports whether they happened to be exactly equal.
 *
 * Headless: raylib is linked, nothing is initialised, no window, no GPU upload.
 * Meshes are built on the stack with only vertices/vertexCount set.
 *
 * Build and run: tests/run_tests.sh
 */

#include <Geometry.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
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

static bool NearF(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

static bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

static bool SameBox(const BoundingBox& a, const BoundingBox& b)
{
	return SameBits(a.min.x, b.min.x) && SameBits(a.min.y, b.min.y) && SameBits(a.min.z, b.min.z)
		&& SameBits(a.max.x, b.max.x) && SameBits(a.max.y, b.max.y) && SameBits(a.max.z, b.max.z);
}

static bool SameVec(const Vector3& a, const Vector3& b)
{
	return SameBits(a.x, b.x) && SameBits(a.y, b.y) && SameBits(a.z, b.z);
}

static bool NearVec(const Vector3& a, const Vector3& b, float tolerance)
{
	return NearF(a.x, b.x, tolerance) && NearF(a.y, b.y, tolerance) && NearF(a.z, b.z, tolerance);
}

static BoundingBox Box(float x0, float y0, float z0, float x1, float y1, float z1)
{
	return BoundingBox{ Vector3{ x0, y0, z0 }, Vector3{ x1, y1, z1 } };
}

static void CompareBoxes(const char* name, BoundingBox a, BoundingBox b)
{
	Check(Geometry::CheckCollisionBoxes(a, b) == ::CheckCollisionBoxes(a, b), std::string("boxes: ") + name);
	Check(Geometry::CheckCollisionBoxes(b, a) == ::CheckCollisionBoxes(b, a), std::string("boxes swapped: ") + name);
}

static void CompareRecs(const char* name, Rectangle a, Rectangle b)
{
	Check(Geometry::CheckCollisionRecs(a, b) == ::CheckCollisionRecs(a, b), std::string("recs: ") + name);
	Check(Geometry::CheckCollisionRecs(b, a) == ::CheckCollisionRecs(b, a), std::string("recs swapped: ") + name);
}

static void ComparePoint(const char* name, Vector2 p, Rectangle r)
{
	Check(Geometry::CheckCollisionPointRec(p, r) == ::CheckCollisionPointRec(p, r), std::string("point-rec: ") + name);
}

static void CompareCircle(const char* name, Vector2 c, float radius, Rectangle r)
{
	Check(Geometry::CheckCollisionCircleRec(c, radius, r) == ::CheckCollisionCircleRec(c, radius, r),
		std::string("circle-rec: ") + name);
}

static int g_exactRays = 0;
static int g_totalRays = 0;

static void CompareRay(const std::string& name, Vector2 pos, Camera3D camera, int w, int h)
{
	const Ray ours = Geometry::GetScreenToWorldRayEx(pos, camera, w, h);
	const Ray theirs = ::GetScreenToWorldRayEx(pos, camera, w, h);
	Check(NearVec(ours.position, theirs.position, 1e-5f), "ray position: " + name);
	Check(NearVec(ours.direction, theirs.direction, 1e-5f), "ray direction: " + name);
	++g_totalRays;
	if (SameVec(ours.position, theirs.position) && SameVec(ours.direction, theirs.direction)) { ++g_exactRays; }
}

static void TestBoxes()
{
	const BoundingBox unit = Box(0, 0, 0, 1, 1, 1);
	CompareBoxes("overlapping", unit, Box(0.5f, 0.5f, 0.5f, 1.5f, 1.5f, 1.5f));
	CompareBoxes("touching on x", unit, Box(1, 0, 0, 2, 1, 1));
	CompareBoxes("touching on y", unit, Box(0, 1, 0, 1, 2, 1));
	CompareBoxes("touching on z", unit, Box(0, 0, 1, 1, 1, 2));
	CompareBoxes("separated on x", unit, Box(1.001f, 0, 0, 2, 1, 1));
	CompareBoxes("separated on y", unit, Box(0, 1.001f, 0, 1, 2, 1));
	CompareBoxes("separated on z", unit, Box(0, 0, 1.001f, 1, 1, 2));
	CompareBoxes("contained", unit, Box(0.25f, 0.25f, 0.25f, 0.75f, 0.75f, 0.75f));
	CompareBoxes("identical", unit, unit);
	CompareBoxes("negative coordinates", Box(-2, -2, -2, -1, -1, -1), Box(-1, -1, -1, 0, 0, 0));
	CompareBoxes("degenerate point on face", unit, Box(1, 0.5f, 0.5f, 1, 0.5f, 0.5f));
	CompareBoxes("zero boxes", Box(0, 0, 0, 0, 0, 0), Box(0, 0, 0, 0, 0, 0));
	CompareBoxes("corner touch", unit, Box(1, 1, 1, 2, 2, 2));

	// Spot-check the expected values too, so a shared bug in both cannot hide
	Check(Geometry::CheckCollisionBoxes(unit, Box(1, 0, 0, 2, 1, 1)), "touching boxes collide");
	Check(!Geometry::CheckCollisionBoxes(unit, Box(1.001f, 0, 0, 2, 1, 1)), "separated boxes do not collide");
}

static void TestRects()
{
	const Rectangle r{ 10, 20, 30, 40 };
	CompareRecs("overlapping", r, Rectangle{ 20, 30, 30, 40 });
	CompareRecs("touching right edge", r, Rectangle{ 40, 20, 10, 40 });
	CompareRecs("touching bottom edge", r, Rectangle{ 10, 60, 30, 10 });
	CompareRecs("separated", r, Rectangle{ 100, 100, 5, 5 });
	CompareRecs("contained", r, Rectangle{ 15, 25, 5, 5 });
	CompareRecs("zero size inside", r, Rectangle{ 20, 30, 0, 0 });

	ComparePoint("inside", Vector2{ 20, 30 }, r);
	ComparePoint("top-left corner", Vector2{ 10, 20 }, r);
	ComparePoint("exactly on right edge", Vector2{ 40, 30 }, r);
	ComparePoint("exactly on bottom edge", Vector2{ 20, 60 }, r);
	ComparePoint("bottom-right corner", Vector2{ 40, 60 }, r);
	ComparePoint("outside", Vector2{ 0, 0 }, r);
	Check(!Geometry::CheckCollisionPointRec(Vector2{ 40, 30 }, r), "point on right edge is outside");
	Check(!Geometry::CheckCollisionPointRec(Vector2{ 20, 60 }, r), "point on bottom edge is outside");
	Check(Geometry::CheckCollisionPointRec(Vector2{ 10, 20 }, r), "point on top-left corner is inside");
}

static void TestCircles()
{
	const Rectangle r{ 0, 0, 10, 10 };
	CompareCircle("centre", Vector2{ 5, 5 }, 1, r);
	CompareCircle("touching right edge", Vector2{ 12, 5 }, 2, r);
	CompareCircle("just short of right edge", Vector2{ 12.01f, 5 }, 2, r);
	CompareCircle("touching bottom edge", Vector2{ 5, 13 }, 3, r);
	CompareCircle("corner exactly touching", Vector2{ 13, 14 }, 5, r);
	CompareCircle("corner just short", Vector2{ 13, 14.01f }, 5, r);
	CompareCircle("corner just inside", Vector2{ 13, 13.99f }, 5, r);
	CompareCircle("top-left corner diagonal", Vector2{ -3, -3 }, 4.2f, r);
	CompareCircle("top-left corner diagonal miss", Vector2{ -3, -3 }, 4.1f, r);
	CompareCircle("far away", Vector2{ 100, 100 }, 1, r);
	CompareCircle("zero radius inside", Vector2{ 5, 5 }, 0, r);
	CompareCircle("zero radius on edge", Vector2{ 10, 5 }, 0, r);
	Check(Geometry::CheckCollisionCircleRec(Vector2{ 13, 14 }, 5, r), "circle touching a corner collides");
	Check(!Geometry::CheckCollisionCircleRec(Vector2{ 13, 14.01f }, 5, r), "circle just short of a corner misses");
}

static void TestMeshBounds()
{
	// Empty mesh: null vertices, whatever the count
	{
		Mesh mesh = { 0 };
		const BoundingBox theirs = ::GetMeshBoundingBox(mesh);
		const BoundingBox ours = Geometry::GetMeshBoundingBox(mesh.vertices, mesh.vertexCount);
		Check(SameBox(ours, theirs), "empty mesh matches raylib");
		Check(SameBox(ours, Box(0, 0, 0, 0, 0, 0)), "empty mesh is the zero box");

		mesh.vertexCount = 5;
		Check(SameBox(Geometry::GetMeshBoundingBox(nullptr, 5), ::GetMeshBoundingBox(mesh)), "null vertices with a count matches raylib");
	}

	// Three vertices
	{
		float vertices[] = { 1.0f, -2.0f, 3.0f,  -4.0f, 5.0f, -6.0f,  0.5f, 0.25f, 8.0f };
		Mesh mesh = { 0 };
		mesh.vertices = vertices;
		mesh.vertexCount = 3;
		const BoundingBox theirs = ::GetMeshBoundingBox(mesh);
		const BoundingBox ours = Geometry::GetMeshBoundingBox(mesh.vertices, mesh.vertexCount);
		Check(SameBox(ours, theirs), "3-vertex mesh matches raylib");
		Check(SameBox(ours, Box(-4, -2, -6, 1, 5, 8)), "3-vertex mesh bounds are the componentwise min and max");
	}

	// One vertex, and a count of zero with vertices present (raylib still reads vertex 0)
	{
		float vertices[] = { 2.0f, 3.0f, 4.0f };
		Mesh mesh = { 0 };
		mesh.vertices = vertices;
		mesh.vertexCount = 1;
		Check(SameBox(Geometry::GetMeshBoundingBox(vertices, 1), ::GetMeshBoundingBox(mesh)), "1-vertex mesh matches raylib");
		mesh.vertexCount = 0;
		Check(SameBox(Geometry::GetMeshBoundingBox(vertices, 0), ::GetMeshBoundingBox(mesh)), "zero count matches raylib");
	}

	// A cube's worth of vertices with negative zero and fractions
	{
		float vertices[] = {
			-0.0f, 0.1f, 0.2f,  0.3f, -0.4f, 0.5f,  -0.6f, 0.7f, -0.8f,  0.9f, -1.0f, 1.1f,
			-1.2f, 1.3f, -1.4f,  1.5f, -1.6f, 1.7f,  0.0f, 0.0f, 0.0f,  -1.8f, 1.9f, -2.0f };
		Mesh mesh = { 0 };
		mesh.vertices = vertices;
		mesh.vertexCount = 8;
		Check(SameBox(Geometry::GetMeshBoundingBox(vertices, 8), ::GetMeshBoundingBox(mesh)), "8-vertex mesh matches raylib");
	}
}

static void TestRays()
{
	const int w = 800;
	const int h = 600;

	Camera3D perspective = { 0 };
	perspective.position = Vector3{ 10.0f, 8.0f, 10.0f };
	perspective.target = Vector3{ 0.0f, 1.0f, 0.0f };
	perspective.up = Vector3{ 0.0f, 1.0f, 0.0f };
	perspective.fovy = 60.0f;
	perspective.projection = CAMERA_PERSPECTIVE;

	Camera3D orthographic = perspective;
	orthographic.fovy = 20.0f;
	orthographic.projection = CAMERA_ORTHOGRAPHIC;

	Camera3D lowAngle = perspective;
	lowAngle.position = Vector3{ -3.5f, 1.7f, 12.25f };
	lowAngle.target = Vector3{ 4.0f, 1.2f, -2.0f };
	lowAngle.fovy = 75.0f;

	const Vector2 spots[] = {
		{ 400, 300 }, { 0, 0 }, { 800, 0 }, { 0, 600 }, { 800, 600 }, { 123.5f, 456.25f }, { 799.9f, 0.1f },
	};
	const char* spotNames[] = { "centre", "top-left", "top-right", "bottom-left", "bottom-right", "inner", "near corner" };

	for (int i = 0; i < 7; ++i)
	{
		CompareRay(std::string("perspective ") + spotNames[i], spots[i], perspective, w, h);
		CompareRay(std::string("orthographic ") + spotNames[i], spots[i], orthographic, w, h);
		CompareRay(std::string("low angle perspective ") + spotNames[i], spots[i], lowAngle, w, h);
	}

	// A different resolution, so width and height are not hard-wired to 800x600
	CompareRay("perspective 1920x1080 centre", Vector2{ 960, 540 }, perspective, 1920, 1080);
	CompareRay("orthographic 1920x1080 corner", Vector2{ 1920, 1080 }, orthographic, 1920, 1080);

	// Sanity: a perspective ray from the centre points at the target and starts at the camera
	const Ray centre = Geometry::GetScreenToWorldRayEx(Vector2{ 400, 300 }, perspective, w, h);
	Check(SameVec(centre.position, perspective.position), "perspective ray starts at the camera");
	const Vector3 toTarget = Vector3Normalize(Vector3Subtract(perspective.target, perspective.position));
	Check(NearVec(centre.direction, toTarget, 1e-4f), "perspective centre ray points at the target");

	std::printf("  rays: %d of %d were bit-identical to raylib's\n", g_exactRays, g_totalRays);
}

static void TestFade()
{
	const Color colors[] = { { 0, 0, 0, 255 }, { 255, 255, 255, 0 }, { 12, 34, 56, 78 }, { 200, 100, 50, 255 } };
	const float alphas[] = { -1.0f, -0.0f, 0.0f, 0.18f, 0.30f, 0.35f, 0.40f, 0.5f, 0.65f, 0.85f, 0.999f, 1.0f, 1.5f, 100.0f };
	bool allSame = true;
	for (const Color& color : colors)
	{
		for (float alpha : alphas)
		{
			const Color ours = Geometry::Fade(color, alpha);
			const Color theirs = ::Fade(color, alpha);
			if (ours.r != theirs.r || ours.g != theirs.g || ours.b != theirs.b || ours.a != theirs.a)
			{
				allSame = false;
				std::printf("  FAIL  Fade alpha %f: ours a=%d, raylib a=%d\n", alpha, ours.a, theirs.a);
			}
		}
	}
	Check(allSame, "Fade matches raylib over 56 colour and alpha pairs");
	Check(Geometry::Fade(Color{ 1, 2, 3, 4 }, 0.5f).a == 127, "Fade 0.5 truncates 127.5 to 127");
	Check(Geometry::Fade(Color{ 1, 2, 3, 4 }, 2.0f).a == 255, "Fade clamps above 1");
	Check(Geometry::Fade(Color{ 1, 2, 3, 4 }, -2.0f).a == 0, "Fade clamps below 0");
}

static void TestExtendBounds()
{
	std::optional<BoundingBox> bounds;
	const BoundingBox first = Box(1, 2, 3, 4, 5, 6);
	Geometry::ExtendBounds(bounds, first);
	Check(bounds.has_value() && SameBox(*bounds, first), "empty optional becomes the added box");

	Geometry::ExtendBounds(bounds, Box(2, 3, 4, 3, 4, 5));
	Check(SameBox(*bounds, first), "a contained box leaves the bounds unchanged");

	Geometry::ExtendBounds(bounds, Box(0, 4, 3, 5, 9, 5));
	Check(SameBox(*bounds, Box(0, 2, 3, 5, 9, 6)), "overlapping box grows the bounds per axis");

	Geometry::ExtendBounds(bounds, Box(-10, -10, -10, -9, -9, -9));
	Check(SameBox(*bounds, Box(-10, -10, -10, 5, 9, 6)), "a disjoint box spans the gap");

	std::optional<BoundingBox> fromMesh;
	float vertices[] = { 1, 1, 1, -1, 2, 0 };
	Geometry::ExtendBounds(fromMesh, Geometry::GetMeshBoundingBox(vertices, 2));
	Geometry::ExtendBounds(fromMesh, Box(0, 0, 0, 3, 0, 0));
	Check(fromMesh.has_value() && SameBox(*fromMesh, Box(-1, 0, 0, 3, 2, 1)), "ExtendBounds composes with GetMeshBoundingBox");
}

int main()
{
	TestBoxes();
	TestRects();
	TestCircles();
	TestMeshBounds();
	TestRays();
	TestFade();
	TestExtendBounds();

	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
