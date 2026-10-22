/**
 * Behavioural tests for Collider3D's two modes, through the scene.
 *
 * The shape maths is covered standalone in tests/ColliderTests.cpp, and the
 * solver itself in tests/PhysicsWorldTests.cpp. What is tested here is the same
 * behaviour with real game objects in a real Scene: that a Collision collider
 * pushes and a Trigger collider does not, while both still report the contact.
 *
 * Each frame runs the objects' own updates and then Scene::SolveCollisions, the
 * same call Scene_updateScene makes, rather than a copy of the solver loop.
 *
 * Like the Stalker FSM tests this links against the engine's own object files,
 * and opens a window because a GameObject entering the map binds a model, which
 * needs a GL context. Run under xvfb on a headless machine.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <Entity.h>
#include <GameObject.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

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

static void CheckEqual(int actual, int expected, const std::string& what)
{
	++g_checks;
	if (actual != expected)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected %d, got %d)\n", what.c_str(), expected, actual);
	}
}

// ─── Fixtures ──────────────────────────────────────────────────────────────

/**
 * An entity that counts the contacts the scene reported for it.
 *
 * It has to be an Entity rather than a GameObject because GameMap::gameObjects
 * stores by value and would slice it. The counts come from the ContactEvents
 * Scene::SolveCollisions returns, so they are what the game itself is told.
 */
struct ProbeEntity : Entity
{
	int collisions = 0;
	int triggerEnters = 0;
	int triggerExits = 0;

	std::unique_ptr<Entity> clone() const override { return std::make_unique<ProbeEntity>(*this); }
};

static SceneManagement::Scene* MakeScene()
{
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	SceneManagement::SceneManager::getInstance().currentScene = scene;
	return scene;
}

static ProbeEntity* AddProbe(SceneManagement::Scene* scene, Vector3 at, std::uint64_t id = 700)
{
	auto owned = std::make_unique<ProbeEntity>();
	owned->id = id;
	owned->rigidBody3D.Teleport(at);
	owned->rigidBody3D.scale = Vector3{ 1, 1, 1 };
	owned->rigidBody3D.SyncBroadPhaseBox();
	// Enabled as a spawned entity would be, then restored so it keeps its id
	owned->onEnable();
	ProbeEntity* probe = owned.get();
	scene->gameMap.RestoreEntity(std::move(owned));
	return probe;
}

// Every floor below is centred at y = -1 with a height of 1, so it occupies
// y in [-1.5, -0.5] and a resting 1-unit body settles at y ~ 0. "Fell through"
// is therefore y < -2: clear underneath with margin, and independent of how
// fast the engine's drag lets things fall.
static GameObject* AddFloor(SceneManagement::Scene* scene, Vector3 centre, Vector3 size, ColliderMode mode,
	std::uint64_t id = 800)
{
	GameObject floor = {};
	floor.id = id;
	floor.name = "TestFloor";
	floor.isEnabled = true;
	floor.rigidBody3D.isStatic = true;
	floor.rigidBody3D.collider.canCollide = true;
	floor.rigidBody3D.translation = centre;
	floor.rigidBody3D.scale = size;
	floor.rigidBody3D.collider.mode = mode;
	floor.rigidBody3D.SyncBroadPhaseBox();
	return scene->gameMap.RestoreGameObject(floor);
}

// One frame in Scene_updateScene's order: bodies integrate in their own
// updates, then the scene resolves every overlapping pair.
static std::vector<Physics3D::ContactEvent> Step(SceneManagement::Scene* scene, float dt)
{
	scene->gameMap.ForEachGameObject([&](GameObject& object) { object.update(scene, dt); });
	scene->gameMap.ForEachEntity([&](Entity& entity) { entity.update(scene, dt); });
	return scene->SolveCollisions(8);
}

// Advances one frame and tallies on the probe what the scene reported between
// it and `other`: every Enter is a collision, and a trigger pair's Enter and
// Exit are also a trigger enter and exit.
static void StepAndTrackContact(SceneManagement::Scene* scene, ProbeEntity* probe, GameObject* other, float dt)
{
	for (const Physics3D::ContactEvent& event : Step(scene, dt))
	{
		const bool isThisPair = (event.a == probe->id && event.b == other->id)
			|| (event.a == other->id && event.b == probe->id);
		if (!isThisPair) { continue; }

		if (event.phase == Physics3D::ContactPhase::Enter)
		{
			++probe->collisions;
			if (event.isTrigger) { ++probe->triggerEnters; }
		}
		else if (event.isTrigger)
		{
			++probe->triggerExits;
		}
	}
}

// ─── Tests ─────────────────────────────────────────────────────────────────

/**
 * The baseline the trigger case is measured against. If this ever fails, the
 * trigger result below means nothing.
 */
static void TestSolidColliderStopsAFall()
{
	std::printf("a collision collider stops a falling body\n");
	SceneManagement::Scene* scene = MakeScene();

	GameObject* floor = AddFloor(scene, Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, COLLIDER_COLLISION);
	ProbeEntity* probe = AddProbe(scene, Vector3{ 0, 2, 0 });

	for (int frame = 0; frame < 120; ++frame)
	{
		StepAndTrackContact(scene, probe, floor, 1.0f / 60.0f);
	}

	Check(probe->rigidBody3D.translation.y > -1.0f,
		"the body is resting on the floor rather than through it");
	Check(probe->collisions > 0, "landing reported a collision");
	CheckEqual(probe->triggerEnters, 0, "a solid contact is not a trigger enter");

	delete scene;
}

static void TestTriggerColliderDoesNotStopAFall()
{
	std::printf("a trigger collider does not stop a falling body\n");
	SceneManagement::Scene* scene = MakeScene();

	// Same floor, same fall, one field different.
	AddFloor(scene, Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, COLLIDER_TRIGGER);
	ProbeEntity* probe = AddProbe(scene, Vector3{ 0, 2, 0 });

	for (int frame = 0; frame < 120; ++frame) { Step(scene, 1.0f / 60.0f); }

	Check(probe->rigidBody3D.translation.y < -2.0f,
		"the body fell clear through the trigger volume and out the bottom");

	delete scene;
}

static void TestTriggerStillReportsTheContact()
{
	std::printf("a trigger reports what passed through it\n");
	SceneManagement::Scene* scene = MakeScene();

	GameObject* floor = AddFloor(scene, Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, COLLIDER_TRIGGER);
	ProbeEntity* probe = AddProbe(scene, Vector3{ 0, 2, 0 });

	for (int frame = 0; frame < 120; ++frame)
	{
		StepAndTrackContact(scene, probe, floor, 1.0f / 60.0f);
	}

	CheckEqual(probe->triggerEnters, 1, "entering the volume fired exactly one onTriggerEnter");
	Check(probe->collisions > 0,
		"onCollision fires for a trigger too, so existing listeners keep working");
	CheckEqual(probe->triggerExits, 1, "leaving the volume fired exactly one onTriggerExit");

	delete scene;
}

/**
 * The reason triggers skip checkRayCollision. A trigger that set downTouch
 * would be a floor the player could stand and jump on — which is exactly what a
 * damage zone or an objective volume must not be.
 */
static void TestTriggerDoesNotGroundABody()
{
	std::printf("a trigger does not make a body grounded\n");
	SceneManagement::Scene* scene = MakeScene();

	GameObject* floor = AddFloor(scene, Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, COLLIDER_TRIGGER);
	ProbeEntity* probe = AddProbe(scene, Vector3{ 0, 0.0f, 0 });

	// Few enough frames that the body is still inside the volume
	for (int frame = 0; frame < 5; ++frame)
	{
		StepAndTrackContact(scene, probe, floor, 1.0f / 60.0f);
	}

	Check(probe->triggerEnters > 0, "the body really is inside the trigger");
	Check(!probe->rigidBody3D.downTouch, "sitting in a trigger does not count as grounded");

	delete scene;
}

/**
 * The collider is a volume of its own, not the render scale. Shrinking it must
 * let two bodies that overlap visually pass without touching.
 */
static void TestColliderSizeChangesWhatCollides()
{
	std::printf("collider size decides contact, not render scale\n");

	// Wide floor, body dropped onto its edge. At full size the body's collider
	// overlaps the floor's; shrunk to a fifth it clears it entirely.
	SceneManagement::Scene* wide = MakeScene();
	AddFloor(wide, Vector3{ 0, -1, 0 }, Vector3{ 4, 1, 4 }, COLLIDER_COLLISION);
	ProbeEntity* onEdge = AddProbe(wide, Vector3{ 2.4f, 2, 0 });
	for (int frame = 0; frame < 120; ++frame) { Step(wide, 1.0f / 60.0f); }
	Check(onEdge->rigidBody3D.translation.y > -1.0f, "the body lands on the floor's edge");
	delete wide;

	SceneManagement::Scene* narrow = MakeScene();
	GameObject* floor = AddFloor(narrow, Vector3{ 0, -1, 0 }, Vector3{ 4, 1, 4 }, COLLIDER_COLLISION);
	// The model still spans 4 units; only its collider shrinks.
	floor->rigidBody3D.collider.size = Vector3{ 0.2f, 1.0f, 0.2f };
	floor->rigidBody3D.SyncBroadPhaseBox();
	ProbeEntity* misses = AddProbe(narrow, Vector3{ 2.4f, 2, 0 });
	for (int frame = 0; frame < 120; ++frame) { Step(narrow, 1.0f / 60.0f); }
	Check(misses->rigidBody3D.translation.y < -2.0f,
		"the same body falls past a floor whose collider was shrunk");
	delete narrow;
}

int main()
{
	// A GameObject entering the map binds a model, which needs a GL context.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-trigger-tests");

	std::printf("Collider mode tests\n\n");

	TestSolidColliderStopsAFall();
	TestTriggerColliderDoesNotStopAFall();
	TestTriggerStillReportsTheContact();
	TestTriggerDoesNotGroundABody();
	TestColliderSizeChangesWhatCollides();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
