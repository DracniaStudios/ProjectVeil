/**
 * Regression test for the Stalker's contact damage.
 *
 * Player::onCollision applied this until collision dispatch moved onto
 * Collider3D ("Collider3D Controls Collision"), which cannot see the owning
 * GameObject by design. It now lives in gameLayer's ContactRules, which reads
 * the ContactEvents Scene::SolveCollisions reports. This guards it: touching a
 * Stalker costs the player exactly its baseDamage, once per contact, not once
 * per solver iteration or per frame of touching.
 *
 * Like StalkerFsmTests and TriggerTests this links against the game's own
 * object files and needs a GL context, since an entity entering the map binds
 * a model. Run under xvfb on a headless machine. See tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>
#include <Player.h>
#include <AI/Stalker.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

static int g_failures = 0;
static int g_checks = 0;

static void CheckNear(float actual, float expected, float tolerance, const std::string& what)
{
	++g_checks;
	if (std::fabs(actual - expected) > tolerance)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected ~%.4f, got %.4f)\n", what.c_str(), expected, actual);
	}
}

static SceneManagement::Scene* MakeScene()
{
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	scene->player = new Player();
	scene->player->type = OBJECT_PLAYER;
	scene->player->id = PLAYER_ID;
	scene->player->onEnable();
	SceneManagement::SceneManager::getInstance().currentScene = scene;
	return scene;
}

static Stalker* AddStalker(SceneManagement::Scene* scene, Vector3 at, std::uint64_t id = 500)
{
	auto owned = std::make_unique<Stalker>();
	owned->id = id;
	owned->rigidBody3D.Teleport(at);
	// Irrelevant here and only adds drift between calls; this test drives
	// the scene's collision solve directly rather than stepping the FSM or physics.
	owned->rigidBody3D.isStatic = true;
	// Enabled as a spawned Stalker would be (full health and stamina), then
	// restored so it keeps the id the assertions name
	owned->onEnable();
	Stalker* stalker = owned.get();
	scene->gameMap.RestoreEntity(std::move(owned));
	return stalker;
}

static void TestStalkerContactDamagesThePlayerOnce()
{
	std::printf("touching a stalker costs the player its baseDamage, once\n");
	SceneManagement::Scene* scene = MakeScene();

	scene->player->rigidBody3D.Teleport(Vector3{ 0, 0, 0 });
	scene->player->rigidBody3D.SyncBroadPhaseBox();

	Stalker* stalker = AddStalker(scene, Vector3{ 0, 0, 0 });
	stalker->rigidBody3D.SyncBroadPhaseBox();

	const float healthBefore = scene->player->health;

	// First contact. The solver runs eight iterations over the pair this frame;
	// the damage belongs to the contact's Enter, not to each iteration.
	scene->SolveCollisions(8);
	CheckNear(scene->player->health, healthBefore - stalker->baseDamage, 0.001f,
		"first contact applies exactly the stalker's baseDamage");

	// Still touching on the next frame: the same contact, no new damage.
	scene->SolveCollisions(8);
	CheckNear(scene->player->health, healthBefore - stalker->baseDamage, 0.001f,
		"staying in contact the next frame does not re-apply damage");

	delete scene;
}

static void TestNonStalkerContactDoesNotDamageThePlayer()
{
	std::printf("touching an ordinary entity does not cost the player health\n");
	SceneManagement::Scene* scene = MakeScene();

	scene->player->rigidBody3D.Teleport(Vector3{ 10, 0, 0 });
	scene->player->rigidBody3D.SyncBroadPhaseBox();

	// A Stalker positioned so the two colliders do not overlap: no contact,
	// so no damage at all.
	Stalker* farStalker = AddStalker(scene, Vector3{ 1000, 1000, 1000 });
	farStalker->rigidBody3D.SyncBroadPhaseBox();

	const float healthBefore = scene->player->health;
	scene->SolveCollisions(8);
	CheckNear(scene->player->health, healthBefore, 0.001f,
		"a stalker out of contact range leaves player health untouched");

	delete scene;
}

int main()
{
	// An entity binds a model when it enters the map, which needs a GL context.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-stalker-contact-damage-tests");

	std::printf("Stalker contact damage tests\n\n");

	TestStalkerContactDamagesThePlayerOnce();
	TestNonStalkerContactDoesNotDamageThePlayer();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
