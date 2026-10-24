/**
 * Tests for the World Editor's picking and gizmo maths.
 *
 * Every click in the viewport is a ray: selection casts it at the scene,
 * placement casts it at surfaces and the ground, and an axis drag solves for
 * the point on the axis nearest to it. When that maths is wrong the editor does
 * not crash, it quietly selects the wrong object or flings one to infinity, so
 * these cases pin down each piece with rays whose answers can be worked out by
 * hand.
 *
 * The free functions in EditorPicking.cpp are pure maths, but PickSceneObject
 * walks a real Scene, so this links against the game's own objects like the
 * other engine-linked suites and needs a GL context for objects entering the
 * map. See tests/run_tests.sh.
 */

#include <raylib.h>
#include <raymath.h>

#include <Scene.h>
#include <SceneManager.h>
#include <WorldEditorTools/EditorGizmo.h>
#include <WorldEditorTools/EditorPicking.h>

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <memory>
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
	if (Vector3Distance(actual, expected) > tolerance)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected (%.3f, %.3f, %.3f), got (%.3f, %.3f, %.3f))\n", what.c_str(),
			expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
	}
}

static Ray MakeRay(Vector3 from, Vector3 direction)
{
	return Ray{ from, direction };
}

// A box in the world: a world object of `size`, as the editor would draw it
static GameObject* AddBox(SceneManagement::Scene* scene, Vector3 at, Vector3 size, std::uint64_t id)
{
	GameObject box = {};
	box.id = id;
	box.name = "TestBox";
	box.rigidBody3D.isStatic = true;
	box.rigidBody3D.translation = at;
	box.rigidBody3D.scale = size;
	return scene->gameMap.RestoreGameObject(box);
}

// ─── Snapping and repair ───────────────────────────────────────────────────

static void TestSnapRoundsToTheNearestStep()
{
	std::printf("snapping rounds to the nearest multiple of the step\n");

	CheckNear(SnapValue(1.2f, 0.5f), 1.0f, 1e-6f, "1.2 snaps down to 1.0");
	CheckNear(SnapValue(1.3f, 0.5f), 1.5f, 1e-6f, "1.3 snaps up to 1.5");
	CheckNear(SnapValue(-1.3f, 0.5f), -1.5f, 1e-6f, "-1.3 snaps to -1.5, the same distance as 1.3");
	CheckNear(SnapValue(44.0f, 15.0f), 45.0f, 1e-6f, "44 degrees snaps to 45 on a 15-degree step");
	CheckNear(SnapValue(3.14159f, 0.0f), 3.14159f, 1e-6f, "a step of 0 leaves the value alone");
	CheckNear(SnapValue(3.14159f, -1.0f), 3.14159f, 1e-6f, "a negative step leaves the value alone");

	CheckVector(SnapVector(Vector3{ 0.26f, -0.74f, 1.01f }, 0.25f), Vector3{ 0.25f, -0.75f, 1.0f }, 1e-5f,
		"a vector snaps each axis on its own");
}

static void TestScaleIsNeverLeftInverted()
{
	std::printf("an editor scale is never left zero or negative\n");

	CheckVector(SanitizeScale(Vector3{ 0.0f, -2.0f, 0.001f }),
		Vector3{ MINIMUM_EDITOR_SCALE, MINIMUM_EDITOR_SCALE, MINIMUM_EDITOR_SCALE }, 1e-7f,
		"zero, negative and tiny extents are raised to the minimum");
	CheckVector(SanitizeScale(Vector3{ 3.0f, 0.5f, 12.0f }), Vector3{ 3.0f, 0.5f, 12.0f }, 1e-7f,
		"ordinary extents are left alone");
}

static void TestADegenerateRotationIsRepaired()
{
	std::printf("a zeroed rotation is repaired to identity, and others are normalised\n");

	const Quaternion repaired = SafeRotation(Quaternion{ 0, 0, 0, 0 });
	Check(repaired.x == 0 && repaired.y == 0 && repaired.z == 0 && repaired.w == 1, "{0,0,0,0} becomes identity");

	const Quaternion scaled = SafeRotation(Quaternion{ 0, 0, 0, 5 });
	CheckNear(QuaternionLength(scaled), 1.0f, 1e-5f, "a non-unit rotation comes back unit length");
	CheckNear(scaled.w, 1.0f, 1e-5f, "pointing the same way");
}

// ─── Planes ────────────────────────────────────────────────────────────────

static void TestARayMeetsAPlane()
{
	std::printf("a ray meets a plane where it crosses it\n");

	Vector3 point = {};
	const bool hit = RayPlane(MakeRay(Vector3{ 0, 10, 0 }, Vector3{ 0, -2, 2 }), Vector3{ 0, 0, 0 }, Vector3{ 0, 1, 0 }, point);

	Check(hit, "a ray falling at 45 degrees meets the ground");
	CheckVector(point, Vector3{ 0, 0, 10 }, 1e-4f, "ten units ahead of where it started");
}

static void TestAParallelOrBehindPlaneIsRefused()
{
	std::printf("a plane the ray runs alongside or has passed is refused\n");

	Vector3 point = { 7, 7, 7 };
	Check(!RayPlane(MakeRay(Vector3{ 0, 1, 0 }, Vector3{ 1, 0, 0 }), Vector3{ 0, 0, 0 }, Vector3{ 0, 1, 0 }, point),
		"a ray parallel to the plane never meets it");
	Check(!RayPlane(MakeRay(Vector3{ 0, 1, 0 }, Vector3{ 0, 1, 0 }), Vector3{ 0, 0, 0 }, Vector3{ 0, 1, 0 }, point),
		"a ray pointing away from the plane never meets it");
	CheckVector(point, Vector3{ 7, 7, 7 }, 0.0f, "and a refused query leaves the output alone");
}

static void TestTheGroundPlaneSitsAtItsHeight()
{
	std::printf("the ground plane is the horizontal plane at the height asked for\n");

	Vector3 point = {};
	Check(RayGroundPlane(MakeRay(Vector3{ 3, 10, -3 }, Vector3{ 0, -1, 0 }), 2.5f, point), "a ray straight down meets it");
	CheckVector(point, Vector3{ 3, 2.5f, -3 }, 1e-4f, "directly below the ray, at y = 2.5");
}

// ─── Axes and points ───────────────────────────────────────────────────────

static void TestClosestPointOnAnAxis()
{
	std::printf("the point on an axis nearest a ray is found by its distance along the axis\n");

	// The camera looks down -Z at x = 3; the X axis runs through the origin
	float t = 0.0f;
	Check(ClosestPointOnAxis(MakeRay(Vector3{ 3, 0, 10 }, Vector3{ 0, 0, -1 }), Vector3{ 0, 0, 0 }, Vector3{ 1, 0, 0 }, t),
		"a ray across the axis has an answer");
	CheckNear(t, 3.0f, 1e-4f, "three units along X");

	// An unnormalised axis still measures in world units
	Check(ClosestPointOnAxis(MakeRay(Vector3{ 3, 0, 10 }, Vector3{ 0, 0, -1 }), Vector3{ 0, 0, 0 }, Vector3{ 4, 0, 0 }, t),
		"a long axis has an answer");
	CheckNear(t, 3.0f, 1e-4f, "measured in world units, not in axis lengths");
}

static void TestAnAxisPointingAtTheCameraIsRefused()
{
	std::printf("an axis seen end-on, or with no length, gives no drag\n");

	float t = 42.0f;
	Check(!ClosestPointOnAxis(MakeRay(Vector3{ 0, 0, 10 }, Vector3{ 0, 0, -1 }), Vector3{ 0, 0, 0 }, Vector3{ 0, 0, 1 }, t),
		"the Z axis seen down its length is refused");
	Check(!ClosestPointOnAxis(MakeRay(Vector3{ 0, 0, 10 }, Vector3{ 0, 0, -1 }), Vector3{ 0, 0, 0 }, Vector3{ 0, 0, 0 }, t),
		"a zero-length axis is refused");
	Check(t == 42.0f, "and a refused query leaves the output alone");
	Check(RayAxisDistance(MakeRay(Vector3{ 0, 0, 10 }, Vector3{ 0, 0, -1 }), Vector3{ 0, 0, 0 }, Vector3{ 0, 0, 1 }, 1.0f) == FLT_MAX,
		"an end-on handle is infinitely far from the cursor");
}

static void TestAHandleIsOnlyAsLongAsItIsDrawn()
{
	std::printf("the distance to a handle is measured to the drawn segment, not the infinite axis\n");

	// The ray passes the X axis at x = 5; the handle only runs to x = 2
	const Ray ray = MakeRay(Vector3{ 5, 0, 10 }, Vector3{ 0, 0, -1 });
	CheckNear(RayAxisDistance(ray, Vector3{ 0, 0, 0 }, Vector3{ 1, 0, 0 }, 2.0f), 3.0f, 1e-4f,
		"a short handle is 3 units from the ray");
	CheckNear(RayAxisDistance(ray, Vector3{ 0, 0, 0 }, Vector3{ 1, 0, 0 }, 10.0f), 0.0f, 1e-4f,
		"a long enough handle is crossed by it");
}

static void TestDistanceFromARayToAPoint()
{
	std::printf("a point's distance from a ray is measured from the eye when it is behind\n");

	const Ray ray = MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, -2 });
	CheckNear(RayPointDistance(ray, Vector3{ 0, 4, -10 }), 4.0f, 1e-4f, "a point ahead is its distance from the line");
	CheckNear(RayPointDistance(ray, Vector3{ 0, 3, 4 }), 5.0f, 1e-4f, "a point behind is its distance from the eye");
	Check(RayPointDistance(MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, 0 }), Vector3{ 1, 1, 1 }) == FLT_MAX,
		"a ray with no direction is infinitely far from everything");
}

// ─── Boxes ─────────────────────────────────────────────────────────────────

static void TestARayHitsTheNearFaceOfABox()
{
	std::printf("a ray hits the near face of a box, with the normal facing back at it\n");

	float distance = 0.0f;
	Vector3 normal = {};
	const bool hit = RayOrientedBox(MakeRay(Vector3{ 0, 0, 10 }, Vector3{ 0, 0, -1 }), Vector3{ 0, 0, 0 },
		Vector3{ 2, 2, 2 }, QuaternionIdentity(), distance, normal);

	Check(hit, "the box straight ahead is hit");
	CheckNear(distance, 9.0f, 1e-4f, "at its +Z face, nine units away");
	CheckVector(normal, Vector3{ 0, 0, 1 }, 1e-4f, "with the normal facing the ray");
}

static void TestARotatedBoxIsPickedByItsOwnShape()
{
	std::printf("a rotated box is hit where it is drawn, not where its bounds are\n");

	// A plank 4 long, 1 tall and 0.2 thick, turned 45 degrees about Y. Its
	// axis-aligned bounds reach about 1.48 from the centre on X and Z.
	const Quaternion turned = QuaternionFromAxisAngle(Vector3{ 0, 1, 0 }, 45.0f * DEG2RAD);
	const Vector3 plank = { 4.0f, 1.0f, 0.2f };

	// 1.41 along the plank's length is on it; 1.41 across it is well off its
	// 0.1 half-thickness, yet still inside those bounds
	const Vector3 onPlank = Vector3RotateByQuaternion(Vector3{ 1.41f, 0, 0 }, turned);
	const Vector3 offPlank = Vector3RotateByQuaternion(Vector3{ 0, 0, 1.41f }, turned);
	Check(std::fabs(offPlank.x) < 1.48f && std::fabs(offPlank.z) < 1.48f, "the miss is aimed inside the plank's bounds");

	float distance = 0.0f;
	Vector3 normal = {};
	Check(!RayOrientedBox(MakeRay(Vector3{ offPlank.x, 10, offPlank.z }, Vector3{ 0, -1, 0 }), Vector3{ 0, 0, 0 },
		plank, turned, distance, normal), "a ray down through the empty corner of its bounds misses");
	Check(RayOrientedBox(MakeRay(Vector3{ onPlank.x, 10, onPlank.z }, Vector3{ 0, -1, 0 }), Vector3{ 0, 0, 0 },
		plank, turned, distance, normal), "a ray down onto the plank itself hits it");
	CheckNear(distance, 9.5f, 1e-3f, "at its top face");
	CheckVector(normal, Vector3{ 0, 1, 0 }, 1e-4f, "which faces up");
}

static void TestARayThatMissesABox()
{
	std::printf("a ray that misses a box reports no hit\n");

	float distance = 0.0f;
	Vector3 normal = {};
	Check(!RayOrientedBox(MakeRay(Vector3{ 5, 0, 10 }, Vector3{ 0, 0, -1 }), Vector3{ 0, 0, 0 }, Vector3{ 2, 2, 2 },
		QuaternionIdentity(), distance, normal), "a ray passing beside the box misses");
	Check(!RayOrientedBox(MakeRay(Vector3{ 0, 0, 10 }, Vector3{ 0, 0, 1 }), Vector3{ 0, 0, 0 }, Vector3{ 2, 2, 2 },
		QuaternionIdentity(), distance, normal), "a ray pointing away from the box misses");
}

// ─── Picking the scene ─────────────────────────────────────────────────────

static void TestTheNearestObjectIsPicked()
{
	std::printf("of two objects along the ray, the nearer is picked\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	AddBox(scene, Vector3{ 0, 0, -10 }, Vector3{ 2, 2, 2 }, 800);
	GameObject* nearer = AddBox(scene, Vector3{ 0, 0, -4 }, Vector3{ 2, 2, 2 }, 801);

	const PickResult pick = PickSceneObject(scene, MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, -1 }), PICK_SELECTABLE);

	Check(pick.hit, "something is picked");
	Check(pick.id == nearer->id && pick.object == nearer, "the nearer box, whatever order they were added in");
	CheckNear(pick.distance, 3.0f, 1e-4f, "at its near face");
	CheckVector(pick.point, Vector3{ 0, 0, -3 }, 1e-4f, "the hit point is on that face");
	CheckVector(pick.normal, Vector3{ 0, 0, 1 }, 1e-4f, "with the normal facing the camera");
	delete scene;
}

static void TestPickingSkipsTheIgnoredObject()
{
	std::printf("the ignored object is looked through, so a placement ghost never lands on itself\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	GameObject* ghost = AddBox(scene, Vector3{ 0, 0, -4 }, Vector3{ 2, 2, 2 }, 810);
	GameObject* behind = AddBox(scene, Vector3{ 0, 0, -10 }, Vector3{ 2, 2, 2 }, 811);

	const PickResult pick = PickSceneObject(scene, MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, -1 }), PICK_SURFACE, ghost->id);
	Check(pick.hit && pick.id == behind->id, "the box behind the ignored one is hit");
	delete scene;
}

static void TestHiddenAndDisabledObjectsCannotBeClicked()
{
	std::printf("an object that is hidden or disabled cannot be clicked\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	GameObject* hidden = AddBox(scene, Vector3{ 0, 0, -4 }, Vector3{ 2, 2, 2 }, 820);
	GameObject* disabled = AddBox(scene, Vector3{ 0, 0, -8 }, Vector3{ 2, 2, 2 }, 821);
	GameObject* visible = AddBox(scene, Vector3{ 0, 0, -12 }, Vector3{ 2, 2, 2 }, 822);
	hidden->display3DModel = false;
	disabled->isEnabled = false;

	const Ray ray = MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, -1 });
	const PickResult selected = PickSceneObject(scene, ray, PICK_SELECTABLE);
	const PickResult surface = PickSceneObject(scene, ray, PICK_SURFACE);

	Check(selected.id == visible->id, "selection looks through both to the visible box");
	Check(surface.id == visible->id, "and so does placement");
	delete scene;
}

static void TestTheFloorIsASurfaceButNotASelection()
{
	std::printf("an unselectable floor can be placed on but not selected\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	GameObject* floor = AddBox(scene, Vector3{ 0, -1, 0 }, Vector3{ 50, 2, 50 }, 830);
	floor->isSelectable = false;

	const Ray down = MakeRay(Vector3{ 3, 10, 3 }, Vector3{ 0, -1, 0 });
	const PickResult selected = PickSceneObject(scene, down, PICK_SELECTABLE);
	const PickResult surface = PickSceneObject(scene, down, PICK_SURFACE);

	Check(!selected.hit, "a click on the floor selects nothing");
	Check(surface.hit && surface.id == floor->id, "but point-and-place lands on it");
	CheckVector(surface.point, Vector3{ 3, 0, 3 }, 1e-4f, "on its top face");
	CheckVector(surface.normal, Vector3{ 0, 1, 0 }, 1e-4f, "facing up");
	delete scene;
}

static void TestEveryKindOfObjectCanBePicked()
{
	std::printf("entities and interactables are picked as well as world geometry\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();

	auto entity = std::make_unique<Entity>();
	entity->id = 840;
	entity->rigidBody3D.isStatic = true;
	entity->rigidBody3D.Teleport(Vector3{ 0, 0, -5 });
	scene->gameMap.RestoreEntity(std::move(entity));

	auto station = std::make_unique<InteractableObject>(INTERACT_MINIGAME, MINI_GAME_CRANE_ID, 0);
	station->id = 841;
	station->rigidBody3D.Teleport(Vector3{ 6, 0, -5 });
	scene->gameMap.RestoreInteractable(std::move(station));

	const PickResult onEntity = PickSceneObject(scene, MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, -1 }), PICK_SELECTABLE);
	const PickResult onStation = PickSceneObject(scene, MakeRay(Vector3{ 6, 0, 0 }, Vector3{ 0, 0, -1 }), PICK_SELECTABLE);

	Check(onEntity.hit && onEntity.id == 840, "the entity is picked");
	Check(onStation.hit && onStation.id == 841, "the Task station is picked");
	delete scene;
}

static void TestPickingNothing()
{
	std::printf("a ray into empty space, or no scene, picks nothing\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	AddBox(scene, Vector3{ 0, 0, -4 }, Vector3{ 2, 2, 2 }, 850);

	const PickResult away = PickSceneObject(scene, MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, 1 }), PICK_SURFACE);
	Check(!away.hit && away.object == nullptr && away.id == 0, "a ray looking away hits nothing");

	const PickResult none = PickSceneObject(nullptr, MakeRay(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, -1 }), PICK_SURFACE);
	Check(!none.hit, "no scene, no pick");
	delete scene;
}

// ─── The gizmo ─────────────────────────────────────────────────────────────

static void TestTheGizmoKeepsItsApparentSize()
{
	std::printf("the gizmo grows with distance so it looks the same size, but never vanishes\n");
	EditorGizmo gizmo;
	Camera3D camera = {};
	camera.position = Vector3{ 0, 0, 0 };

	const float closer = gizmo.HandleLength(camera, Vector3{ 0, 0, -10 });
	const float farther = gizmo.HandleLength(camera, Vector3{ 0, 0, -20 });
	CheckNear(farther, closer * 2.0f, 1e-4f, "twice as far away, twice as long");
	Check(gizmo.HandleLength(camera, Vector3{ 0, 0, 0 }) > 0.0f, "a camera on the pivot still has a grabbable gizmo");
}

static void TestTheGizmoColoursItsAxes()
{
	std::printf("each axis has its own colour, and a highlight overrides it\n");

	const Color x = GizmoAxisColor(0, false);
	const Color y = GizmoAxisColor(1, false);
	const Color z = GizmoAxisColor(2, false);
	Check(x.r > x.g && x.r > x.b, "X is red");
	Check(y.g > y.r && y.g > y.b, "Y is green");
	Check(z.b > z.r && z.b > z.g, "Z is blue");

	const Color lit = GizmoAxisColor(1, true);
	Check(lit.r != y.r || lit.g != y.g || lit.b != y.b, "a highlighted axis changes colour");
	const Color uniform = GizmoAxisColor(-1, false);
	Check(uniform.r == WHITE.r && uniform.g == WHITE.g && uniform.b == WHITE.b, "the uniform-scale block is white");
}

static void TestSelectModeNeverTakesTheMouse()
{
	std::printf("in Select mode the gizmo never takes the mouse or moves anything\n");
	EditorGizmo gizmo;
	gizmo.mode = GIZMO_SELECT;

	Camera3D camera = {};
	camera.position = Vector3{ 0, 2, 6 };
	camera.target = Vector3{ 0, 0, 0 };
	camera.up = Vector3{ 0, 1, 0 };
	camera.fovy = 60.0f;
	camera.projection = CAMERA_PERSPECTIVE;

	GizmoTransform transform = {};
	transform.position = Vector3{ 1, 2, 3 };

	Check(!gizmo.Update(camera, transform, true), "Update reports the mouse as free");
	Check(!gizmo.IsDragging() && !gizmo.IsHovered(), "nothing is hovered or dragged");
	Check(!gizmo.DragBegan() && !gizmo.DragEnded(), "and no drag edge fires");
	CheckVector(transform.position, Vector3{ 1, 2, 3 }, 0.0f, "the transform is untouched");
}

static void TestCancelDropsAnyGrab()
{
	std::printf("cancelling leaves the gizmo with nothing grabbed\n");
	EditorGizmo gizmo;

	gizmo.Cancel();
	Check(!gizmo.IsDragging() && !gizmo.IsHovered(), "nothing is hovered or dragged");
	Check(!gizmo.DragBegan() && !gizmo.DragEnded(), "and no drag edge fires");
}

int main()
{
	// An object binds a model when it enters the map, which needs a GL context.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-editor-picking-tests");

	std::printf("Editor picking tests\n\n");

	TestSnapRoundsToTheNearestStep();
	TestScaleIsNeverLeftInverted();
	TestADegenerateRotationIsRepaired();

	TestARayMeetsAPlane();
	TestAParallelOrBehindPlaneIsRefused();
	TestTheGroundPlaneSitsAtItsHeight();

	TestClosestPointOnAnAxis();
	TestAnAxisPointingAtTheCameraIsRefused();
	TestAHandleIsOnlyAsLongAsItIsDrawn();
	TestDistanceFromARayToAPoint();

	TestARayHitsTheNearFaceOfABox();
	TestARotatedBoxIsPickedByItsOwnShape();
	TestARayThatMissesABox();

	TestTheNearestObjectIsPicked();
	TestPickingSkipsTheIgnoredObject();
	TestHiddenAndDisabledObjectsCannotBeClicked();
	TestTheFloorIsASurfaceButNotASelection();
	TestEveryKindOfObjectCanBePicked();
	TestPickingNothing();

	TestTheGizmoKeepsItsApparentSize();
	TestTheGizmoColoursItsAxes();
	TestSelectModeNeverTakesTheMouse();
	TestCancelDropsAnyGrab();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
