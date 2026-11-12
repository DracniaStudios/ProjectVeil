/**
 * Behavioural tests for the Stalker's 4-state FSM.
 *
 * The Vertical slice's acceptance criteria are claims about *why* the stalker
 * moved — "driven only by sound events, with no position feed" — which cannot
 * be checked by watching it move. These drive the FSM with synthetic
 * SoundEvents and assert the locked transitions directly.
 *
 * Unlike SoundFieldTests this needs the engine, because Stalker::update runs
 * Entity::update -> GameObject::update with the Scene it is handed, and reads
 * that scene's map and sound field. It links against the game's
 * own object files (every one except the translation unit holding main) so no
 * build restructuring is required. See tests/run_tests.sh.
 *
 * A window is opened because an entity binds a model when it enters the map,
 * which needs a GL context. Run under xvfb on a headless machine.
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
#include <Client/StalkerWarnings.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <Client/GamePresentation.h>

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
static SceneManagement::Scene* MakeScene()
{
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	return scene;
}

static Stalker* AddStalker(SceneManagement::Scene* scene, Vector3 at, std::uint64_t id = 500)
{
	auto owned = std::make_unique<Stalker>();
	owned->id = id;
	owned->rigidBody3D.Teleport(at);
	// Gravity is irrelevant here and only adds drift between steps; the FSM
	// steers and measures on the XZ plane regardless of height.
	owned->rigidBody3D.isStatic = true;
	// Enabled as a spawned Stalker would be (full health and stamina), then
	// restored so it keeps the id the assertions name
	owned->onEnable();
	Stalker* stalker = owned.get();
	scene->gameMap.RestoreEntity(std::move(owned));
	return stalker;
}

// One simulated frame: age the field first, exactly as Scene_tick does,
// so an event emitted this step is audible on this step.
static void Step(SceneManagement::Scene* scene, Stalker* stalker, float dt)
{
	scene->soundField.Update(dt);
	stalker->update(scene, dt);
}

// Solid world geometry for the avoidance cases. Only GameMap::gameObjects are
// probed, which is where level geometry lives.
static BoundingBox AddBlock(SceneManagement::Scene* scene, Vector3 centre, Vector3 size)
{
	GameObject block = {};
	block.name = "TestBlock";
	block.isEnabled = true;
	block.rigidBody3D.isStatic = true;
	block.rigidBody3D.collider.canCollide = true;
	block.rigidBody3D.translation = centre;
	block.rigidBody3D.scale = size;
	block.rigidBody3D.SyncBroadPhaseBox();
	GameObject* spawned = scene->gameMap.RestoreGameObject(block);
	return spawned->rigidBody3D.broadPhaseBox;
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	scene->soundField.Emit(Vector3{ 1, 0, 0 }, 1.0f, SOUND_TAMPER, 999);
	Step(scene, s, 0.016f);

	CheckState(*s, STALKER_HUNT, "a loud noise escalates straight to a hunt");
	delete scene;
}

static void TestBeliefIsASnapshotNotATrack()
{
	std::printf("belief is a snapshot, not a position feed\n");
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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
	SceneManagement::Scene* scene = MakeScene();
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

// G1 (MultiplayerSplitPlan.md, Q32): the catch is checked outside the FSM, so
// a player within reach is caught in every one of the four states, and the
// FSM's own update never looks at the player at all.
static void TestContactCatchesInEveryState()
{
	std::printf("contact raises a catch in every state, outside the FSM\n");

	static_assert(STALKER_STATE_COUNT == 4, "the FSM is locked at four states");

	for (int s = 0; s < STALKER_STATE_COUNT; ++s)
	{
		const StalkerState state = static_cast<StalkerState>(s);
		const std::string name = stalkerStateToString(state);

		SceneManagement::Scene* scene = MakeScene();
		scene->AddPlayer(0);
		scene->PlayerInSlot(0)->type = OBJECT_PLAYER;
		scene->PlayerInSlot(0)->onEnable();
		scene->PlayerInSlot(0)->rigidBody3D.Teleport(Vector3{ 0.5f, 0, 0 });

		Stalker* stalker = AddStalker(scene, Vector3{ 0, 0, 0 });
		stalker->state = state;
		stalker->lastKnownPosition = Vector3{ 3, 0, 0 };
		stalker->hasLastKnown = true;

		scene->CheckCatches();
		Check(scene->CaughtThisTick().size() == 1 && scene->CaughtThisTick()[0] == Sim::PlayerIdForSlot(0), "a player within catchRadius is caught in " + name);

		scene->CheckCatches();
		Check(scene->CaughtThisTick().size() == 1, "and checking twice in one tick is still one catch (" + name + ")");

		// The FSM runs without ever reading the player: the update leaves the
		// state where sound alone would take it, and raises nothing itself
		SceneManagement::Scene* empty = MakeScene();
		Stalker* alone = AddStalker(empty, Vector3{ 0, 0, 0 });
		alone->state = state;
		Step(empty, alone, 0.016f);
		Check(empty->CaughtThisTick().empty(), "the FSM's update raises no catch itself (" + name + ")");
		delete empty;

		scene->RemovePlayer(0);
		delete scene;
	}

	// Out of reach is not a catch, in any state
	SceneManagement::Scene* scene = MakeScene();
	scene->AddPlayer(0);
	scene->PlayerInSlot(0)->type = OBJECT_PLAYER;
	scene->PlayerInSlot(0)->onEnable();
	scene->PlayerInSlot(0)->rigidBody3D.Teleport(Vector3{ 4, 0, 0 });
	Stalker* stalker = AddStalker(scene, Vector3{ 0, 0, 0 });
	stalker->state = STALKER_HUNT;
	scene->CheckCatches();
	Check(scene->CaughtThisTick().empty(), "a player out of reach is not caught");

	// A disabled stalker catches nobody
	scene->PlayerInSlot(0)->rigidBody3D.Teleport(Vector3{ 0.5f, 0, 0 });
	stalker->isEnabled = false;
	scene->CheckCatches();
	Check(scene->CaughtThisTick().empty(), "a disabled stalker catches nobody");

	scene->RemovePlayer(0);
	delete scene;
}

// G2 (Stalker FSM Decisions Log 8 and 11): a search walks to the last known
// position first and its timer starts on arrival. A position the stalker can't
// reach is given up on where progress stops, and the search starts there.
static void TestUnreachableLastKnownFallsBack()
{
	std::printf("an unreachable last known position: search from the nearest reachable point\n");
	SceneManagement::Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });

	// The noise is behind a wall the stalker can't get through: it is held in
	// place by teleporting it back each step, which is what a wall does to it
	scene->soundField.Emit(Vector3{ 3, 0, 0 }, 0.2f, SOUND_FOOTSTEP, 999);
	Step(scene, s, 0.016f);
	CheckState(*s, STALKER_INVESTIGATE, "investigating the noise");

	const Vector3 stuckAt = Vector3{ 1, 0, 0 };
	float waited = 0.0f;
	for (int i = 0; i < 400 && s->state == STALKER_INVESTIGATE; ++i)
	{
		s->rigidBody3D.Teleport(stuckAt);
		Step(scene, s, 0.016f);
		waited += 0.016f;
	}

	CheckState(*s, STALKER_SEARCH_LAST_KNOWN, "a stalker that can't reach the noise starts searching");
	Check(waited < Stalker::kStallSeconds + 1.0f, "within a couple of seconds of getting stuck (waited " + std::to_string(waited) + " s)");
	Check(s->searchArrived, "the search counts as arrived");
	Check(Vector3Distance(s->searchAnchor, stuckAt) < 0.5f, "anchored at the nearest point it reached, not at the noise");
	Check(s->searchTime < 0.1f, "and the search timer starts there");
	delete scene;
}

static void TestSearchTimerStartsOnArrival()
{
	std::printf("the search timer starts only on arrival at the last known position\n");
	SceneManagement::Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });
	s->rigidBody3D.isStatic = false;
	s->rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f);

	s->state = STALKER_HUNT;
	s->lastKnownPosition = Vector3{ 6, 0, 0 };
	s->hasLastKnown = true;
	s->lastStimulusTime = -100.0f; // the trail is already cold

	Step(scene, s, 0.016f);
	CheckState(*s, STALKER_SEARCH_LAST_KNOWN, "a cold hunt becomes a search");
	Check(!s->searchArrived, "which first walks to the last known position");

	// Walking there takes longer than nothing; the timer must not have run
	for (int i = 0; i < 30; ++i)
	{
		Step(scene, s, 0.016f);
		s->rigidBody3D.Update(0.016f);
	}
	Check(s->searchArrived || s->searchTime == 0.0f, "no search time passes before arrival");

	for (int i = 0; i < 600 && !s->searchArrived; ++i)
	{
		Step(scene, s, 0.016f);
		s->rigidBody3D.Update(0.016f);
	}
	Check(s->searchArrived, "it arrives");
	Check(Vector3Distance(s->searchAnchor, Vector3{ 6, 0, 0 }) < 0.01f, "a reachable position anchors the sweep itself");
	CheckState(*s, STALKER_SEARCH_LAST_KNOWN, "and is still searching on arrival");
	delete scene;
}

// G2, the Director's hint: one Patrol detour to the hinted region, then back
// to the route. Ignored in every other state, and dropped by anything heard.
static void TestDirectorHintIsOnePatrolDetour()
{
	std::printf("a Director hint is one patrol detour\n");
	SceneManagement::Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });
	s->rigidBody3D.isStatic = false;
	s->rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f);
	s->waypoints = { Vector3{ -10, 0, 0 }, Vector3{ -10, 0, 10 } };

	s->ApplyDirectorHint(Vector3{ 8, 0, 0 }, 0.5f);
	Check(s->HasHint(), "a hint is taken in patrol");

	// Heads for the hint rather than waypoint 0, which lies the other way
	const float startX = s->getPosition().x;
	for (int i = 0; i < 20; ++i) { Step(scene, s, 0.016f); s->rigidBody3D.Update(0.016f); }
	Check(s->getPosition().x > startX, "the patrol detours toward the hinted region");

	for (int i = 0; i < 600 && s->HasHint(); ++i) { Step(scene, s, 0.016f); s->rigidBody3D.Update(0.016f); }
	Check(!s->HasHint(), "the hint is used up on arrival");
	CheckState(*s, STALKER_PATROL, "and the stalker is still patrolling");

	// Every other state ignores a hint
	for (const StalkerState other : { STALKER_INVESTIGATE, STALKER_HUNT, STALKER_SEARCH_LAST_KNOWN })
	{
		SceneManagement::Scene* scene2 = MakeScene();
		Stalker* t = AddStalker(scene2, Vector3{ 0, 0, 0 });
		t->state = other;
		t->ApplyDirectorHint(Vector3{ 8, 0, 0 }, 0.5f);
		Check(!t->HasHint(), std::string("a hint is ignored in ") + stalkerStateToString(other));
		delete scene2;
	}

	// Hearing something mid-detour drops the hint
	s->ApplyDirectorHint(Vector3{ 8, 0, 0 }, 0.5f);
	scene->soundField.Emit(s->getPosition() + Vector3{ 1, 0, 0 }, 0.2f, SOUND_FOOTSTEP, 999);
	Step(scene, s, 0.016f);
	CheckState(*s, STALKER_INVESTIGATE, "a noise mid-detour starts an investigation");
	Check(!s->HasHint(), "and the hint is dropped");
	delete scene;
}

// G4 (decisions 15, 16): a station starting is heard room-wide and investigated
// faster; it is never a reason to hunt
static void TestStationActivationIsAFastInvestigation()
{
	std::printf("a station starting is heard room-wide and investigated at task speed\n");

	// Far away and behind a wall: an ordinary noise wouldn't get through
	SceneManagement::Scene* scene = MakeScene();
	Stalker* s = AddStalker(scene, Vector3{ 0, 0, 0 });
	AddBlock(scene, Vector3{ 30, 0, 0 }, Vector3{ 1, 5, 20 });
	scene->soundField.Emit(Vector3{ 60, 0, 0 }, 0.8f, SOUND_STATION_ACTIVATION, 999);
	Step(scene, s, 0.016f);
	CheckState(*s, STALKER_INVESTIGATE, "a station 60 units away behind a wall is still heard: investigate");
	Check(s->investigatingActivation, "as an activation");

	SceneManagement::Scene* quiet = MakeScene();
	Stalker* q = AddStalker(quiet, Vector3{ 0, 0, 0 });
	AddBlock(quiet, Vector3{ 30, 0, 0 }, Vector3{ 1, 5, 20 });
	quiet->soundField.Emit(Vector3{ 60, 0, 0 }, 0.8f, SOUND_FOOTSTEP, 999);
	Step(quiet, q, 0.016f);
	CheckState(*q, STALKER_PATROL, "the same noise as a footstep is not heard");
	delete quiet;

	Check(scene->soundField.AudibleLoudnessAt(Vector3{ 0, 0, 0 }, scene->soundField.Events().back(), &scene->gameMap) == 0.8f, "an activation reaches the stalker at full loudness");
	delete scene;

	// Investigated faster than an ordinary noise of the same loudness
	const auto distanceAfter = [](SoundKind kind) {
		SceneManagement::Scene* sc = MakeScene();
		Stalker* st = AddStalker(sc, Vector3{ 0, 0, 0 });
		st->rigidBody3D.isStatic = false;
		st->rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f);
		sc->soundField.Emit(Vector3{ 8, 0, 0 }, kind == SOUND_FOOTSTEP ? 0.3f : 0.8f, kind, 999);
		for (int i = 0; i < 30; ++i) { Step(sc, st, 0.016f); st->rigidBody3D.Update(0.016f); }
		const float travelled = st->getPosition().x;
		const bool investigating = st->state == STALKER_INVESTIGATE;
		delete sc;
		return investigating ? travelled : -1.0f;
	};
	const float normal = distanceAfter(SOUND_FOOTSTEP);
	const float task = distanceAfter(SOUND_STATION_ACTIVATION);
	Check(normal > 0.0f && task > normal * 1.3f, "an activation is investigated at taskSpeedMultiplier (" + std::to_string(task) + " vs " + std::to_string(normal) + ")");
}

// G4 (decision 6): warnings start at catchRadius + closingSpeed * lead
static void TestWarningRadius()
{
	std::printf("the audio warnings start at the decision 6 radius\n");
	using Client::StalkerWarning;
	Check(Client::WarningRadius(1.0f, 4.0f, 1.5f) == 7.0f, "1 + 4 * 1.5 = 7");
	Check(Client::WarningFor(6.9f, 4.0f, 6.0f, 1.0f, 1.5f) == StalkerWarning::Footsteps, "a moving stalker inside its radius: footsteps");
	Check(Client::WarningFor(7.1f, 4.0f, 6.0f, 1.0f, 1.5f) == StalkerWarning::None, "just outside: nothing");
	Check(Client::WarningFor(12.0f, 10.0f, 6.0f, 1.0f, 1.5f) == StalkerWarning::Footsteps, "a faster stalker is heard from farther away");
	Check(Client::WarningFor(9.9f, 0.0f, 6.0f, 1.0f, 1.5f) == StalkerWarning::Breathing, "a standing stalker: breathing, at the player's walk speed");
	Check(Client::WarningFor(10.1f, 0.0f, 6.0f, 1.0f, 1.5f) == StalkerWarning::None, "outside that: nothing");
}

int main()
{
	// An entity binds a model when it enters the map, so a GL context has to
	// exist before any is added.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-fsm-tests");
	Client::InstallGamePresentation();

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
	TestContactCatchesInEveryState();
	TestUnreachableLastKnownFallsBack();
	TestSearchTimerStartsOnArrival();
	TestDirectorHintIsOnePatrolDetour();
	TestStationActivationIsAFastInvestigation();
	TestWarningRadius();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
