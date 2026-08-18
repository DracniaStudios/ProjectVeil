/**
 * Behavioural tests for the Stalker's 4-state FSM.
 *
 * The Vertical slice's acceptance criteria are claims about *why* the stalker
 * moved — "driven only by sound events, with no position feed" — which cannot
 * be checked by watching it move. These drive the FSM with synthetic
 * SoundEvents and assert the locked transitions directly.
 *
 * Unlike SoundFieldTests this needs the engine, because Stalker::update runs
 * Entity::update -> GameObject::update -> RigidBody3D::UpdateForce, which
 * reaches SceneManager::currentScene->gameMap. It links against the game's
 * own object files (every one except the translation unit holding main) so no
 * build restructuring is required. See tests/run_tests.sh.
 *
 * A window is opened because GameObject's constructor uploads a fallback cube
 * mesh, which needs a GL context. Run under xvfb on a headless machine.
 *
 * Transitions are exercised by placing the stalker rather than waiting for it
 * to walk: this is a test of the state machine, not of steering or physics
 * timing, and placing keeps every case deterministic.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <AI/Stalker.h>
#include <Perception/SoundField.h>

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

static void CheckState(const Stalker& s, StalkerState expected, const std::string& what)
{
	++g_checks;
	if (s.state != expected)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected %s, got %s)\n",
			what.c_str(), stalkerStateToString(expected), stalkerStateToString(s.state));
	}
}

// A scene with no geometry: the sound field's occlusion pass walks
// gameMap.gameObjects, and leaving it empty keeps every case about the FSM
// rather than about level layout.
static Scene* MakeScene()
{
	Scene* scene = new Scene();
	SceneManager::getInstance().currentScene = scene;
	return scene;
}

static Stalker* AddStalker(Scene* scene, Vector3 at, std::uint64_t id = 500)
{
	auto owned = std::make_unique<Stalker>();
	owned->id = id;
	owned->rigidBody3D.Teleport(at);
	// Gravity is irrelevant here and only adds drift between steps; the FSM
	// steers and measures on the XZ plane regardless of height.
	owned->rigidBody3D.isStatic = true;
	Stalker* stalker = owned.get();
	scene->entities[id] = std::move(owned);
	return stalker;
}

// One simulated frame: age the field first, exactly as Scene_updateScene does,
// so an event emitted this step is audible on this step.
static void Step(Scene* scene, Stalker* stalker, float dt)
{
	scene->soundField.Update(dt);
	stalker->update(scene, dt);
}

// Solid world geometry for the avoidance cases. Only GameMap::gameObjects are
// probed, which is where level geometry lives.
static BoundingBox AddBlock(Scene* scene, Vector3 centre, Vector3 size)
{
	GameObject block = {};
	block.name = "TestBlock";
	block.isEnabled = true;
	block.rigidBody3D.isStatic = true;
	block.rigidBody3D.canCollide = true;
	block.rigidBody3D.translation = centre;
	block.rigidBody3D.scale = size;
	block.rigidBody3D.SyncCollisionBox();
	scene->gameMap.gameObjects.push_back(block);
	return scene->gameMap.gameObjects.back().rigidBody3D.collisionBox;
}

static bool InsideBox(Vector3 p, const BoundingBox& box)
{
	return p.x >= box.min.x && p.x <= box.max.x &&
	       p.y >= box.min.y && p.y <= box.max.y &&
	       p.z >= box.min.z && p.z <= box.max.z;
}

static void TestPatrolToInvestigateOnQuietNoise()
{
	std::printf("patrol -> investigate on a quiet noise\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	CheckState(*s, STALKER_PATROL, "starts in patrol");

	// Above hearing threshold, below hunt threshold, and close enough that
	// distance falloff leaves it in that band.
	scene->soundField.Emit(Vector3{ 2, 0, 0 }, 0.2f, SOUND_FOOTSTEP, 999);
	Step(scene, s, 0.016f);

	CheckState(*s, STALKER_INVESTIGATE, "a quiet noise opens an investigation");
	delete scene;
}

static void TestPatrolToHuntOnLoudNoise()
{
	std::printf("patrol -> hunt on a loud noise\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	scene->soundField.Emit(Vector3{ 1, 0, 0 }, 1.0f, SOUND_TAMPER, 999);
	Step(scene, s, 0.016f);

	CheckState(*s, STALKER_HUNT, "a loud noise escalates straight to a hunt");
	delete scene;
}

static void TestBeliefIsASnapshotNotATrack()
{
	std::printf("belief is a snapshot, not a position feed\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	const Vector3 noiseAt = Vector3{ 5, 0, 5 };
	scene->soundField.Emit(noiseAt, 1.0f, SOUND_FOOTSTEP, 999);
	Step(scene, s, 0.016f);

	Check(s->hasLastKnown, "hearing a noise establishes a belief");
	Check(std::fabs(s->lastKnownPosition.x - noiseAt.x) < 0.001f &&
		std::fabs(s->lastKnownPosition.z - noiseAt.z) < 0.001f,
		"the belief is the position the noise was emitted from");

	// This is the locked decision under test. The emitter has "moved", but it
	// made no new sound, so the stalker must still believe the old position —
	// anything else means something is feeding it live positions.
	for (int i = 0; i < 30; ++i) { Step(scene, s, 0.016f); }

	Check(std::fabs(s->lastKnownPosition.x - noiseAt.x) < 0.001f &&
		std::fabs(s->lastKnownPosition.z - noiseAt.z) < 0.001f,
		"the belief does not track anything after the noise stops");
	delete scene;
}

static void TestIgnoresItsOwnNoise()
{
	std::printf("stalker ignores its own emissions\n");
	Scene* scene = MakeScene();
	constexpr std::uint64_t kId = 77;
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 }, kId);

	// Emitted under the stalker's own id: without the ignore-id the stalker
	// hears itself and chases its own footsteps around the room.
	scene->soundField.Emit(Vector3{ 1, 0, 0 }, 1.0f, SOUND_FOOTSTEP, kId);
	Step(scene, s, 0.016f);

	CheckState(*s, STALKER_PATROL, "its own noise does not move it out of patrol");
	Check(!s->hasLastKnown, "its own noise does not establish a belief");
	delete scene;
}

static void TestInvestigateToSearchOnArrival()
{
	std::printf("investigate -> search on arrival\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	scene->soundField.Emit(Vector3{ 3, 0, 0 }, 0.2f, SOUND_FOOTSTEP, 999);
	Step(scene, s, 0.016f);
	CheckState(*s, STALKER_INVESTIGATE, "investigating");

	// Placed on the noise at a different height, which is the case the 3D
	// arrival check used to fail: planar distance is 0 but the 3D distance is
	// 4, so the old comparison never registered arrival and the stalker sat
	// on the spot forever.
	s->rigidBody3D.Teleport(Vector3{ 3, 4, 0 });

	// Let the event expire so nothing re-triggers investigate on arrival.
	for (int i = 0; i < 400 && s->state == STALKER_INVESTIGATE; ++i)
	{
		Step(scene, s, 0.016f);
		s->rigidBody3D.Teleport(Vector3{ 3, 4, 0 });
	}

	CheckState(*s, STALKER_SEARCH_LAST_KNOWN, "arriving with nothing there starts a search");
	delete scene;
}

static void TestHuntDecaysToSearchOnStimulusAge()
{
	std::printf("hunt -> search when the trail goes cold\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	scene->soundField.Emit(Vector3{ 1, 0, 0 }, 1.0f, SOUND_TAMPER, 999);
	Step(scene, s, 0.016f);
	CheckState(*s, STALKER_HUNT, "hunting");

	// Moved far away so the noise is inaudible rather than deleted: the hunt
	// must decay on the *age* of the last stimulus, and an event still within
	// earshot would legitimately keep refreshing it.
	s->rigidBody3D.Teleport(Vector3{ 500, 0, 500 });

	const float patience = s->huntPatience;
	for (float t = 0.0f; t <= patience + 1.0f && s->state == STALKER_HUNT; t += 0.05f)
	{
		Step(scene, s, 0.05f);
		s->rigidBody3D.Teleport(Vector3{ 500, 0, 500 });
	}

	CheckState(*s, STALKER_SEARCH_LAST_KNOWN, "a stale hunt decays to a search");
	delete scene;
}

static void TestSearchGivesUpToPatrol()
{
	std::printf("search -> patrol on giving up\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	scene->soundField.Emit(Vector3{ 1, 0, 0 }, 1.0f, SOUND_TAMPER, 999);
	Step(scene, s, 0.016f);
	s->rigidBody3D.Teleport(Vector3{ 500, 0, 500 });

	const float budget = s->huntPatience + s->searchDuration + 2.0f;
	for (float t = 0.0f; t <= budget && s->state != STALKER_PATROL; t += 0.05f)
	{
		Step(scene, s, 0.05f);
		s->rigidBody3D.Teleport(Vector3{ 500, 0, 500 });
	}

	CheckState(*s, STALKER_PATROL, "an exhausted search returns to patrol");
	Check(!s->hasLastKnown, "giving up clears the stale belief");
	delete scene;
}

static void TestWaypointAdvanceIsHeightIndependent()
{
	std::printf("patrol advances waypoints regardless of height\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	// Route authored well above the body, as "Add Waypoint At Player" produces
	// when the two bodies do not share a centre height. The 3D arrival check
	// could never satisfy this; the planar one does.
	s->waypoints = {
		Vector3{ 0, 6, 0 },
		Vector3{ 10, 6, 0 },
		Vector3{ 10, 6, 10 },
	};
	s->currentWaypoint = 0;

	Step(scene, s, 0.016f);
	Check(s->currentWaypoint == 1, "standing on waypoint 0 advances to waypoint 1");

	s->rigidBody3D.Teleport(Vector3{ 10, 0, 0 });
	Step(scene, s, 0.016f);
	Check(s->currentWaypoint == 2, "reaching waypoint 1 advances to waypoint 2");

	s->rigidBody3D.Teleport(Vector3{ 10, 0, 10 });
	Step(scene, s, 0.016f);
	Check(s->currentWaypoint == 0, "the route is a closed loop back to waypoint 0");
	delete scene;
}

static void TestClearPathIsNotDeflected()
{
	std::printf("a clear path is steered straight\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });
	s->waypoints = { Vector3{ 10, 0, 0 } };
	s->currentWaypoint = 0;

	Step(scene, s, 0.016f);

	const Vector3 v = s->rigidBody3D.GetVelocity();
	Check(v.x > 0.0f, "moves toward the waypoint");
	Check(std::fabs(v.z) < 0.001f, "with nothing in the way it does not deviate");
	delete scene;
}

static void TestSteersAsideForAnObstacle()
{
	std::printf("an obstacle ahead deflects the heading\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });
	s->waypoints = { Vector3{ 10, 0, 0 } };
	s->currentWaypoint = 0;

	// A pillar squarely on the straight line, inside the probe distance.
	AddBlock(scene, Vector3{ 2, 0, 0 }, Vector3{ 1, 4, 1 });

	Step(scene, s, 0.016f);

	const Vector3 v = s->rigidBody3D.GetVelocity();
	Check(std::fabs(v.z) > 0.001f, "the heading is deflected off the blocked line");
	Check(v.x > 0.0f, "and still makes progress toward the target rather than reversing");
	delete scene;
}

static void TestRoutesAroundAnObstacleWithoutEnteringIt()
{
	std::printf("routes around an obstacle rather than through it\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });
	const Vector3 goal = Vector3{ 12, 0, 0 };
	s->waypoints = { goal };
	s->currentWaypoint = 0;

	const BoundingBox pillar = AddBlock(scene, Vector3{ 6, 0, 0 }, Vector3{ 2, 4, 2 });

	// Integrated by hand rather than through the solver: the point is where the
	// AI steers, and a hand-integrated body passes straight through geometry if
	// the steering does not avoid it. That makes the containment check below a
	// real assertion instead of one the collision solver would satisfy anyway.
	bool everInside = false;
	bool arrived = false;
	constexpr float dt = 0.05f;

	for (int i = 0; i < 2000; ++i)
	{
		Step(scene, s, dt);

		const Vector3 v = s->rigidBody3D.GetVelocity();
		Vector3 p = s->getPosition();
		p.x += v.x * dt;
		p.z += v.z * dt;
		s->rigidBody3D.Teleport(p);

		if (InsideBox(p, pillar)) { everInside = true; }
		if (std::fabs(p.x - goal.x) < 1.5f && std::fabs(p.z - goal.z) < 3.0f) { arrived = true; break; }
	}

	Check(!everInside, "the path never enters the obstacle");
	Check(arrived, "and still reaches the far side");
	delete scene;
}

static void TestAvoidanceIsSkippedWithoutAWorld()
{
	std::printf("no world means no avoidance\n");
	Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	// A block that would deflect the heading if the map were consulted. Passing
	// no map is how the steering behaved before avoidance existed, and it has to
	// stay a straight line so a stalker driven without a world still works.
	AddBlock(scene, Vector3{ 2, 0, 0 }, Vector3{ 1, 4, 1 });

	Check(s->DistanceToObstruction(Vector3{ 1, 0, 0 }, 3.0f, &scene->gameMap) < 3.0f,
		"the block is detected when the map is supplied");
	Check(s->DistanceToObstruction(Vector3{ 1, 0, 0 }, 3.0f, nullptr) == 3.0f,
		"and reports clear when it is not");
	delete scene;
}

int main()
{
	// GameObject's constructor uploads a fallback cube, so a GL context has to
	// exist before any entity is built.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-fsm-tests");

	std::printf("Stalker FSM tests\n\n");

	TestPatrolToInvestigateOnQuietNoise();
	TestPatrolToHuntOnLoudNoise();
	TestBeliefIsASnapshotNotATrack();
	TestIgnoresItsOwnNoise();
	TestInvestigateToSearchOnArrival();
	TestHuntDecaysToSearchOnStimulusAge();
	TestSearchGivesUpToPatrol();
	TestWaypointAdvanceIsHeightIndependent();
	TestClearPathIsNotDeflected();
	TestSteersAsideForAnObstacle();
	TestRoutesAroundAnObstacleWithoutEnteringIt();
	TestAvoidanceIsSkippedWithoutAWorld();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
