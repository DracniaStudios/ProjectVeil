/**
 * Regression test for the Stalker's contact damage.
 *
 * Player::onCollision applied this until collision dispatch moved onto
 * Collider3D ("Collider3D Controls Collision"), which cannot see the owning
 * GameObject by design (see the comment above Collider3D::onCollisionEnter).
 * The behaviour was reconnected at the one call site that still has both real
 * GameObjects — RigidBody3D::resolveConstrains's ApplyStalkerContactDamage.
 * This guards that reconnection: touching a Stalker costs the player exactly
 * its baseDamage, once per contact, not once per solver iteration.
 *
 * Like StalkerFsmTests and TriggerTests this links against the game's own
 * object files and needs a GL context for GameObject's fallback cube mesh.
 * Run under xvfb on a headless machine. See tests/run_tests.sh.
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

static Scene* MakeScene()
{
	Scene* scene = new Scene();
	scene->player = new Player();
	scene->player->type = OBJECT_PLAYER;
	scene->player->id = PLAYER_ID;
	scene->player->onEnable();
	SceneManager::getInstance().currentScene = scene;
	return scene;
}

static Stalker* AddStalker(Scene* scene, Vector3 at, std::uint64_t id = 500)
{
	auto owned = std::make_unique<Stalker>();
	owned->id = id;
	owned->rigidBody3D.Teleport(at);
	// Irrelevant here and only adds drift between calls; this test drives
	// resolveConstrains directly rather than stepping the FSM or physics.
	owned->rigidBody3D.isStatic = true;
	Stalker* stalker = owned.get();
	scene->gameMap.LoadEntity(std::move(owned));
	return stalker;
}

static void TestStalkerContactDamagesThePlayerOnce()
{
	std::printf("touching a stalker costs the player its baseDamage, once\n");
	Scene* scene = MakeScene();

	scene->player->rigidBody3D.Teleport(Vector3{ 0, 0, 0 });
	scene->player->rigidBody3D.SyncBroadPhaseBox();

	Stalker* stalker = AddStalker(scene, Vector3{ 0, 0, 0 });
	stalker->rigidBody3D.SyncBroadPhaseBox();

	const float healthBefore = scene->player->health;

	// First contact this frame.
	scene->player->rigidBody3D.resolveConstrains(scene->player, stalker);
	CheckNear(scene->player->health, healthBefore - stalker->baseDamage, 0.001f,
		"first contact applies exactly the stalker's baseDamage");

	// The solver runs several iterations per overlapping pair per frame;
	// resolveConstrains must not re-apply damage on each one.
	scene->player->rigidBody3D.resolveConstrains(scene->player, stalker);
	CheckNear(scene->player->health, healthBefore - stalker->baseDamage, 0.001f,
		"a second solver iteration in the same frame does not re-apply damage");

	delete scene;
}

static void TestNonStalkerContactDoesNotDamageThePlayer()
{
	std::printf("touching an ordinary entity does not cost the player health\n");
	Scene* scene = MakeScene();

	scene->player->rigidBody3D.Teleport(Vector3{ 10, 0, 0 });
	scene->player->rigidBody3D.SyncBroadPhaseBox();

	// A Stalker positioned so the two colliders do not overlap: no contact,
	// no call to ApplyStalkerContactDamage at all.
	Stalker* farStalker = AddStalker(scene, Vector3{ 1000, 1000, 1000 });
	farStalker->rigidBody3D.SyncBroadPhaseBox();

	const float healthBefore = scene->player->health;
	scene->player->rigidBody3D.resolveConstrains(scene->player, farStalker);
	CheckNear(scene->player->health, healthBefore, 0.001f,
		"a stalker out of contact range leaves player health untouched");

	delete scene;
}

int main()
{
	// GameObject's constructor uploads a fallback cube mesh, which needs a GL
	// context.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-stalker-contact-damage-tests");

	std::printf("Stalker contact damage tests\n\n");

	TestStalkerContactDamagesThePlayerOnce();
	TestNonStalkerContactDoesNotDamageThePlayer();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
