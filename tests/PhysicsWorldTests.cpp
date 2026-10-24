/**
 * Behavioural tests for Physics3D::PhysicsWorld::Solve, the collision half of a
 * physics frame.
 *
 * Cheap tier, like the Collider suite: PhysicsWorld.cpp, RigidBody3D.cpp and
 * Collider3D.cpp have no engine dependency, so this links against raylib alone,
 * needs no window and builds no GameObject. The bodies are bare RigidBody3Ds
 * with ids, which is all the world ever sees. See tests/run_tests.sh.
 */

#include <PhysicsWorld.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace Physics3D;

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
		std::printf("  FAIL  %s (expected %.3f, got %.3f)\n", what.c_str(), expected, actual);
	}
}

// ─── Fixtures ──────────────────────────────────────────────────────────────

// The player's shape. A touch ray leaves a face and reaches half the body's
// height past it, and a resting body sits a little inside what it rests on, so
// a body this tall is grounded on the 1-unit floors used here.
constexpr Vector3 kPlayerSize = { 1, 2, 1 };

static RigidBody3D MakeBox(Vector3 at, Vector3 size, bool isStatic)
{
	RigidBody3D body{};
	body.Teleport(at);
	body.scale = size;
	body.isStatic = isStatic;
	body.SyncBroadPhaseBox();
	return body;
}

// One frame in Scene_tick's order: every body integrates, then the world
// resolves what overlaps.
static std::vector<ContactEvent> Frame(PhysicsWorld& world, const std::vector<PhysicsBody>& bodies,
	float dt = 1.0f / 60.0f, int iterations = 8)
{
	for (const PhysicsBody& body : bodies) { body.rigidBody->Update(dt); }
	return world.Solve(bodies, iterations);
}

static int CountOf(const std::vector<ContactEvent>& events, ContactPhase phase)
{
	int count = 0;
	for (const ContactEvent& event : events) { if (event.phase == phase) { ++count; } }
	return count;
}

// ─── Solid contacts ────────────────────────────────────────────────────────

static void TestABodyComesToRestOnAFloorAndIsGrounded()
{
	std::printf("a falling body comes to rest on a floor and is grounded there\n");
	PhysicsWorld world;
	RigidBody3D floor = MakeBox(Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, true);
	RigidBody3D box = MakeBox(Vector3{ 0, 3, 0 }, kPlayerSize, false);
	const std::vector<PhysicsBody> bodies = { { 10, &floor }, { 20, &box } };

	Frame(world, bodies);
	Check(!box.downTouch, "a body in mid-air is not grounded");

	for (int frame = 0; frame < 120; ++frame) { Frame(world, bodies); }

	Check(box.translation.y > 0.3f && box.translation.y < 0.7f, "the body rests on top of the floor");
	Check(box.downTouch, "a resting body is grounded");
	Check(!box.upTouch, "with nothing above it");
}

static void TestOneEnterPerContactNamingBothBodies()
{
	std::printf("landing is one Enter, naming both bodies, and resting adds nothing\n");
	PhysicsWorld world;
	RigidBody3D floor = MakeBox(Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, true);
	RigidBody3D box = MakeBox(Vector3{ 0, 2, 0 }, Vector3{ 1, 1, 1 }, false);
	// The dynamic body has the lower id here, so the order the bodies are listed
	// in can't be what decides a and b
	const std::vector<PhysicsBody> bodies = { { 20, &floor }, { 10, &box } };

	int enters = 0;
	int exits = 0;
	ContactEvent landing{};
	for (int frame = 0; frame < 120; ++frame)
	{
		for (const ContactEvent& event : Frame(world, bodies))
		{
			if (event.phase == ContactPhase::Enter) { ++enters; landing = event; }
			else { ++exits; }
		}
	}

	Check(enters == 1, "one landing is one Enter");
	Check(exits == 0, "resting on the floor never reports an Exit");
	Check(landing.a == 10 && landing.b == 20, "the Enter names both bodies, lower id first");
	Check(!landing.isTrigger, "a solid landing is not a trigger contact");
	Check(world.IsTouching(10, 20) && world.IsTouching(20, 10), "the pair is still touching, asked either way round");
}

static void TestEnterMeasuresTheImpactBeforeResolvingIt()
{
	std::printf("an Enter carries the normal, closing speed and faster body from before the push\n");
	PhysicsWorld world;
	RigidBody3D wall = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, true);
	RigidBody3D crate = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, false);
	crate.SetVelocity(Vector3{ -10, 0, 0 });
	const std::vector<PhysicsBody> bodies = { { 30, &wall }, { 31, &crate } };

	const std::vector<ContactEvent> events = world.Solve(bodies);
	Check(events.size() == 1 && events[0].phase == ContactPhase::Enter, "the hit is one Enter");
	if (events.size() != 1) { return; }

	CheckNear(events[0].normal.x, 1.0f, 0.001f, "the normal points from the wall (a) toward the crate (b)");
	CheckNear(events[0].closingSpeed, 10.0f, 0.01f, "the closing speed is the crate's speed into the wall");
	Check(events[0].faster == 31, "the crate is the faster body");
	Check(crate.GetVelocity().x > -10.0f, "and the crate was still stopped by the wall");

	// Swapping the ids flips which body is a, and with it the normal; the speed
	// of the impact doesn't change.
	PhysicsWorld swapped;
	RigidBody3D wall2 = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, true);
	RigidBody3D crate2 = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, false);
	crate2.SetVelocity(Vector3{ -10, 0, 0 });
	const std::vector<PhysicsBody> swappedBodies = { { 41, &wall2 }, { 40, &crate2 } };
	const std::vector<ContactEvent> swappedEvents = swapped.Solve(swappedBodies);
	Check(swappedEvents.size() == 1, "the swapped pair also reports one Enter");
	if (swappedEvents.size() != 1) { return; }
	CheckNear(swappedEvents[0].normal.x, -1.0f, 0.001f, "the normal points from the crate (now a) toward the wall");
	CheckNear(swappedEvents[0].closingSpeed, 10.0f, 0.01f, "the closing speed is the same");
	Check(swappedEvents[0].faster == 40, "the crate is still the faster body");
}

static void TestEveryIterationOfOneContactIsOneEnter()
{
	std::printf("eight solver iterations over one contact report it once\n");
	PhysicsWorld world;
	RigidBody3D a = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, false);
	RigidBody3D b = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, false);
	const std::vector<PhysicsBody> bodies = { { 1, &a }, { 2, &b } };

	Check(CountOf(world.Solve(bodies, 8), ContactPhase::Enter) == 1, "one Enter for eight iterations");
	Check(world.Solve(bodies, 8).empty(), "and nothing more while they stay in contact");
	Check(a.translation.x < 0.0f && b.translation.x > 0.5f, "both dynamic bodies were pushed apart");
}

static void TestBodiesThatSeparateReportAnExit()
{
	std::printf("bodies that stop touching report one Exit\n");
	PhysicsWorld world;
	RigidBody3D a = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, true);
	RigidBody3D b = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, false);
	const std::vector<PhysicsBody> bodies = { { 1, &a }, { 2, &b } };
	world.Solve(bodies);

	b.Teleport(Vector3{ 50, 0, 0 });
	b.SyncBroadPhaseBox();
	const std::vector<ContactEvent> events = world.Solve(bodies);

	Check(events.size() == 1 && events[0].phase == ContactPhase::Exit, "moving apart is one Exit");
	Check(!events.empty() && events[0].a == 1 && events[0].b == 2, "naming both bodies");
	Check(!world.IsTouching(1, 2), "and the pair no longer touches");
}

static void TestABodyThatLeavesTheWorldReportsAnExit()
{
	std::printf("a body that leaves the world ends its contacts with an Exit\n");
	PhysicsWorld world;
	RigidBody3D a = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, true);
	RigidBody3D b = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, false);
	world.Solve(std::vector<PhysicsBody>{ { 1, &a }, { 2, &b } });

	const std::vector<ContactEvent> events = world.Solve(std::vector<PhysicsBody>{ { 1, &a } });
	Check(events.size() == 1 && events[0].phase == ContactPhase::Exit, "the destroyed body's contact is one Exit");
}

static void TestGroundedOnAnySolidBody()
{
	std::printf("a body standing on another dynamic body is grounded "
		"(regression: touch flags came only from plain GameObjects)\n");
	PhysicsWorld world;
	RigidBody3D floor = MakeBox(Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, true);
	RigidBody3D lower = MakeBox(Vector3{ 0, 0.5f, 0 }, Vector3{ 2, 1, 2 }, false);
	RigidBody3D upper = MakeBox(Vector3{ 0, 3, 0 }, kPlayerSize, false);
	const std::vector<PhysicsBody> bodies = { { 1, &floor }, { 2, &lower }, { 3, &upper } };

	for (int frame = 0; frame < 180; ++frame) { Frame(world, bodies); }

	Check(upper.translation.y > lower.translation.y + 1.0f, "the upper body is resting on the lower one");
	Check(upper.downTouch, "and is grounded on it");
}

// ─── Triggers ──────────────────────────────────────────────────────────────

static void TestATriggerReportsButNeverPushesOrGrounds()
{
	std::printf("a trigger reports what passes through it, and never stops or grounds it\n");
	PhysicsWorld world;
	RigidBody3D volume = MakeBox(Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, true);
	volume.collider.mode = COLLIDER_TRIGGER;
	RigidBody3D box = MakeBox(Vector3{ 0, 2, 0 }, Vector3{ 1, 1, 1 }, false);
	const std::vector<PhysicsBody> bodies = { { 1, &volume }, { 2, &box } };

	int enters = 0;
	int exits = 0;
	bool allTriggers = true;
	bool everGrounded = false;
	for (int frame = 0; frame < 120; ++frame)
	{
		for (const ContactEvent& event : Frame(world, bodies))
		{
			if (event.phase == ContactPhase::Enter) { ++enters; } else { ++exits; }
			allTriggers = allTriggers && event.isTrigger;
		}
		everGrounded = everGrounded || box.downTouch;
	}

	Check(box.translation.y < -2.0f, "the body fell clear through the trigger");
	Check(enters == 1 && exits == 1, "it entered once and left once");
	Check(allTriggers, "both contacts say they were a trigger's");
	Check(!everGrounded, "passing through the trigger never grounded it");
}

// ─── Who takes part ────────────────────────────────────────────────────────

static void TestInactiveBodiesTakeNoPart()
{
	std::printf("a body that is disabled or can't collide takes no part\n");

	const auto overlapsWith = [](auto disable) {
		PhysicsWorld world;
		RigidBody3D a = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, false);
		RigidBody3D b = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, false);
		disable(b);
		const std::vector<ContactEvent> events = world.Solve(std::vector<PhysicsBody>{ { 1, &a }, { 2, &b } });
		return !events.empty() || a.translation.x != 0.0f;
	};

	Check(!overlapsWith([](RigidBody3D& body) { body.collider.canCollide = false; }),
		"a body that can't collide is neither reported nor pushed against");
	Check(!overlapsWith([](RigidBody3D& body) { body.isEnabled = false; }),
		"nor is a disabled one");

	PhysicsWorld world;
	RigidBody3D floor = MakeBox(Vector3{ 0, -1, 0 }, Vector3{ 20, 1, 20 }, true);
	floor.collider.canCollide = false;
	// Where a body resting on this floor would sit, if the floor took part
	RigidBody3D box = MakeBox(Vector3{ 0, 0.49f, 0 }, kPlayerSize, false);
	world.Solve(std::vector<PhysicsBody>{ { 1, &floor }, { 2, &box } });
	Check(!box.downTouch, "a floor that can't collide doesn't ground anything");
	floor.collider.canCollide = true;
	world.Solve(std::vector<PhysicsBody>{ { 1, &floor }, { 2, &box } });
	Check(box.downTouch, "the same floor does once it can");
}

static void TestTwoStaticBodiesNeverTouch()
{
	std::printf("two static bodies never touch\n");
	PhysicsWorld world;
	RigidBody3D a = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, true);
	RigidBody3D b = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, true);

	Check(world.Solve(std::vector<PhysicsBody>{ { 1, &a }, { 2, &b } }).empty(), "overlapping statics report nothing");
	Check(a.translation.x == 0.0f && b.translation.x == 0.5f, "and neither moves");
}

static void TestResetForgetsContacts()
{
	std::printf("Reset forgets every contact, so a pair still overlapping enters again\n");
	PhysicsWorld world;
	RigidBody3D a = MakeBox(Vector3{ 0, 0, 0 }, Vector3{ 1, 1, 1 }, true);
	RigidBody3D b = MakeBox(Vector3{ 0.5f, 0, 0 }, Vector3{ 1, 1, 1 }, false);
	const std::vector<PhysicsBody> bodies = { { 1, &a }, { 2, &b } };
	world.Solve(bodies);

	world.Reset();
	Check(!world.IsTouching(1, 2), "nothing is touching after a Reset");
	Check(CountOf(world.Solve(bodies), ContactPhase::Enter) == 1, "the pair enters again");
}

int main()
{
	std::printf("PhysicsWorld tests\n\n");

	TestABodyComesToRestOnAFloorAndIsGrounded();
	TestOneEnterPerContactNamingBothBodies();
	TestEnterMeasuresTheImpactBeforeResolvingIt();
	TestEveryIterationOfOneContactIsOneEnter();
	TestBodiesThatSeparateReportAnExit();
	TestABodyThatLeavesTheWorldReportsAnExit();
	TestGroundedOnAnySolidBody();
	TestATriggerReportsButNeverPushesOrGrounds();
	TestInactiveBodiesTakeNoPart();
	TestTwoStaticBodiesNeverTouch();
	TestResetForgetsContacts();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
