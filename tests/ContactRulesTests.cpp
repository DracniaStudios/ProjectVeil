/**
 * Tests for ContactRules: what a contact means for the game.
 *
 * PhysicsWorld reports which pairs started or stopped touching; ContactRules
 * turns those reports into noise, Stalker contact damage and projectile hits.
 * These cases hand Apply the reports directly, so each rule is tested on its
 * own terms rather than on whatever the solver happened to produce: which side
 * of a pair a body is on, whether the contact is starting or ending, and what
 * happens when a body named in a report is already gone.
 *
 * The solver-driven versions live in EmitterTests (impact noise) and
 * StalkerContactDamageTests (damage once per contact). Like them this links
 * against the game's own objects and needs a GL context, since an object binds
 * a model when it enters the map. See tests/run_tests.sh.
 */

#include <raylib.h>

#include <ContactRules.h>
#include <Scene.h>
#include <SceneManager.h>
#include <Player.h>
#include <AI/Stalker.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using Physics3D::ContactEvent;
using Physics3D::ContactPhase;

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

static SceneManagement::Scene* MakeScene()
{
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	scene->player = new Player();
	scene->player->type = OBJECT_PLAYER;
	scene->player->id = PLAYER_ID;
	scene->player->onEnable(); // full health
	return scene;
}

static GameObject* AddObject(SceneManagement::Scene* scene, ObjectType type, Vector3 at, std::uint64_t id,
                             float baseDamage = 1.0f)
{
	GameObject object = {};
	object.id = id;
	object.name = "TestObject";
	object.type = type;
	object.baseDamage = baseDamage;
	object.rigidBody3D.Teleport(at);
	return scene->gameMap.RestoreGameObject(object);
}

static Entity* AddEntity(SceneManagement::Scene* scene, std::unique_ptr<Entity> owned, std::uint64_t id)
{
	owned->id = id;
	owned->rigidBody3D.isStatic = true;
	owned->onEnable(); // full health and stamina
	return scene->gameMap.RestoreEntity(std::move(owned));
}

static ContactEvent Enter(std::uint64_t a, std::uint64_t b, float closingSpeed = 0.0f, std::uint64_t faster = 0)
{
	ContactEvent event = {};
	event.a = a;
	event.b = b;
	event.phase = ContactPhase::Enter;
	event.closingSpeed = closingSpeed;
	event.faster = faster == 0 ? a : faster;
	return event;
}

static void Apply(SceneManagement::Scene* scene, std::vector<ContactEvent> events)
{
	ContactRules::Apply(*scene, events);
}

static int ImpactCount(const SceneManagement::Scene* scene)
{
	int count = 0;
	for (const SoundEvent& event : scene->soundField.Events())
	{
		if (event.kind == SOUND_IMPACT) { ++count; }
	}
	return count;
}

static const SoundEvent* FirstImpact(const SceneManagement::Scene* scene)
{
	for (const SoundEvent& event : scene->soundField.Events())
	{
		if (event.kind == SOUND_IMPACT) { return &event; }
	}
	return nullptr;
}

// ─── Projectiles ───────────────────────────────────────────────────────────

static void TestAProjectileHurtsThePlayerAndIsSpent()
{
	std::printf("a projectile hitting the player costs it the projectile's damage, and is spent\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* projectile = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 600, 2.5f);
	const float before = scene->player->health;

	Apply(scene, { Enter(PLAYER_ID, projectile->id) });

	CheckNear(scene->player->health, before - 2.5f, 0.001f, "the player loses the projectile's baseDamage");
	Check(projectile->pendingDestroy && !projectile->isAlive, "the projectile is spent");
	delete scene;
}

static void TestAProjectileHurtsAnEntityFromEitherSide()
{
	std::printf("a projectile hurts an entity whichever side of the contact it is on\n");
	SceneManagement::Scene* scene = MakeScene();
	Entity* target = AddEntity(scene, std::make_unique<Entity>(), 700);
	GameObject* first = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 601, 1.0f);
	GameObject* second = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 602, 3.0f);
	const float before = target->health;

	Apply(scene, { Enter(first->id, target->id) });
	CheckNear(target->health, before - 1.0f, 0.001f, "the projectile as `a` hurts the entity");

	Apply(scene, { Enter(target->id, second->id) });
	CheckNear(target->health, before - 4.0f, 0.001f, "the projectile as `b` hurts it too");

	Check(first->pendingDestroy && second->pendingDestroy, "both projectiles are spent");
	delete scene;
}

static void TestAProjectileStrikingTheWorldHurtsNothing()
{
	std::printf("a projectile striking world geometry damages nothing and flies on\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* wall = AddObject(scene, OBJECT_GENERIC, Vector3{ 0, 0, 0 }, 610);
	GameObject* item = AddObject(scene, OBJECT_ITEM, Vector3{ 2, 0, 0 }, 611);
	GameObject* projectile = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 612, 5.0f);
	const float playerBefore = scene->player->health;

	Apply(scene, { Enter(wall->id, projectile->id), Enter(projectile->id, item->id) });

	Check(!wall->pendingDestroy && !item->pendingDestroy, "neither the wall nor the item is destroyed");
	Check(!projectile->pendingDestroy, "the projectile is not spent on scenery; it decays on its own");
	CheckNear(scene->player->health, playerBefore, 0.001f, "and the player is untouched");
	delete scene;
}

static void TestTwoProjectilesDoNotHurtEachOther()
{
	std::printf("two projectiles meeting do nothing to each other\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* a = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 620);
	GameObject* b = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 621);

	Apply(scene, { Enter(a->id, b->id) });
	Check(!a->pendingDestroy && !b->pendingDestroy, "neither is spent");
	delete scene;
}

// ─── Stalker contact ───────────────────────────────────────────────────────

static void TestTheStalkerHurtsThePlayerFromEitherSide()
{
	std::printf("the Stalker catching the player hurts it whichever side of the contact it is on\n");
	SceneManagement::Scene* scene = MakeScene();
	Entity* stalker = AddEntity(scene, std::make_unique<Stalker>(), 710);
	stalker->baseDamage = 2.0f;
	const float before = scene->player->health;

	Apply(scene, { Enter(stalker->id, PLAYER_ID) });
	CheckNear(scene->player->health, before - 2.0f, 0.001f, "the Stalker as `a` costs its baseDamage");

	Apply(scene, { Enter(PLAYER_ID, stalker->id) });
	CheckNear(scene->player->health, before - 4.0f, 0.001f, "the Stalker as `b` costs it again, for a new contact");

	CheckNear(stalker->health, stalker->getMaxHealth(), 0.001f, "the Stalker takes nothing back");
	delete scene;
}

static void TestOnlyTheStalkerHurtsOnContact()
{
	std::printf("touching an entity that is not the Stalker costs the player nothing\n");
	SceneManagement::Scene* scene = MakeScene();
	Entity* plain = AddEntity(scene, std::make_unique<Entity>(), 720);
	plain->baseDamage = 5.0f;
	const float before = scene->player->health;

	Apply(scene, { Enter(PLAYER_ID, plain->id) });
	CheckNear(scene->player->health, before, 0.001f, "a plain entity does no contact damage");

	// The Stalker touching something other than the player hurts nobody
	Entity* stalker = AddEntity(scene, std::make_unique<Stalker>(), 721);
	Apply(scene, { Enter(stalker->id, plain->id) });
	CheckNear(plain->health, plain->getMaxHealth(), 0.001f, "the Stalker brushing another entity hurts neither");
	delete scene;
}

// ─── Phases and missing bodies ─────────────────────────────────────────────

static void TestOnlyTheStartOfAContactCounts()
{
	std::printf("a contact ending applies no rule\n");
	SceneManagement::Scene* scene = MakeScene();
	Entity* stalker = AddEntity(scene, std::make_unique<Stalker>(), 730);
	GameObject* projectile = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 731);
	GameObject* crate = AddObject(scene, OBJECT_GENERIC, Vector3{ 3, 0, 0 }, 732);
	const float before = scene->player->health;

	std::vector<ContactEvent> exits = {
		Enter(stalker->id, PLAYER_ID),
		Enter(projectile->id, PLAYER_ID),
		Enter(crate->id, PLAYER_ID, 20.0f),
	};
	for (ContactEvent& event : exits) { event.phase = ContactPhase::Exit; }
	Apply(scene, exits);

	CheckNear(scene->player->health, before, 0.001f, "no damage on Exit");
	Check(!projectile->pendingDestroy, "no projectile spent on Exit");
	Check(ImpactCount(scene) == 0, "no impact noise on Exit");
	delete scene;
}

static void TestContactsWithMissingBodiesAreSkipped()
{
	std::printf("a contact naming a body that is gone is skipped, and the rest still apply\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* projectile = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 740, 1.5f);
	GameObject* gone = AddObject(scene, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 741, 9.0f);
	const std::uint64_t goneId = gone->id;

	scene->gameMap.Destroy(goneId);
	scene->gameMap.SweepDestroyed(scene);
	Check(scene->gameMap.FindWorldObject(goneId) == nullptr, "the destroyed projectile has left the map");

	const float before = scene->player->health;
	Apply(scene, { Enter(goneId, PLAYER_ID), Enter(PLAYER_ID, 99999), Enter(projectile->id, PLAYER_ID) });

	CheckNear(scene->player->health, before - 1.5f, 0.001f, "only the projectile still in the world does damage");

	// A scene with no player at all
	SceneManagement::Scene* empty = new SceneManagement::Scene();
	GameObject* stray = AddObject(empty, OBJECT_PROJECTILE, Vector3{ 0, 1, 0 }, 742);
	Apply(empty, { Enter(PLAYER_ID, stray->id) });
	Check(!stray->pendingDestroy, "with no player, a contact naming the player is skipped");

	delete empty;
	delete scene;
}

// ─── Impact noise ──────────────────────────────────────────────────────────

static void TestImpactLoudnessFollowsClosingSpeed()
{
	std::printf("impact loudness rises with closing speed and is capped at full\n");

	const struct { float speed; float loudness; const char* what; } cases[] = {
		{ 2.0f, 0.0f, "2 u/s is too quiet to hear" },
		{ 7.0f, 0.5f, "7 u/s is half loudness" },
		{ 12.0f, 1.0f, "12 u/s is full loudness" },
		{ 60.0f, 1.0f, "60 u/s is still only full loudness" },
	};

	for (const auto& c : cases)
	{
		SceneManagement::Scene* scene = MakeScene();
		GameObject* floor = AddObject(scene, OBJECT_GENERIC, Vector3{ 0, 0, 0 }, 750);
		GameObject* crate = AddObject(scene, OBJECT_GENERIC, Vector3{ 0, 1, 0 }, 751);

		Apply(scene, { Enter(floor->id, crate->id, c.speed, crate->id) });

		const SoundEvent* impact = FirstImpact(scene);
		if (c.loudness == 0.0f)
		{
			Check(impact == nullptr, c.what);
		}
		else
		{
			Check(impact != nullptr, c.what);
			if (impact != nullptr) { CheckNear(impact->loudness, c.loudness, 0.001f, c.what); }
		}
		delete scene;
	}
}

static void TestImpactIsCreditedToTheFasterBody()
{
	std::printf("the impact is heard where the faster body is, and credited to it\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* floor = AddObject(scene, OBJECT_GENERIC, Vector3{ 0, 0, 0 }, 760);
	GameObject* crate = AddObject(scene, OBJECT_GENERIC, Vector3{ 4, 1, -2 }, 761);

	Apply(scene, { Enter(floor->id, crate->id, 10.0f, crate->id) });

	const SoundEvent* impact = FirstImpact(scene);
	Check(impact != nullptr, "an impact is heard");
	if (impact != nullptr)
	{
		Check(impact->sourceId == crate->id, "credited to the crate, not the floor it struck");
		Check(impact->position.x == 4.0f && impact->position.z == -2.0f, "at the crate's position");
	}
	delete scene;
}

static void TestATriggerIsPassedThroughSilently()
{
	std::printf("a trigger is passed through, not struck, so it makes no impact noise\n");
	SceneManagement::Scene* scene = MakeScene();
	GameObject* zone = AddObject(scene, OBJECT_GENERIC, Vector3{ 0, 0, 0 }, 770);
	GameObject* crate = AddObject(scene, OBJECT_GENERIC, Vector3{ 0, 1, 0 }, 771);
	Entity* stalker = AddEntity(scene, std::make_unique<Stalker>(), 772);
	const float before = scene->player->health;

	ContactEvent throughZone = Enter(zone->id, crate->id, 30.0f, crate->id);
	throughZone.isTrigger = true;
	ContactEvent caught = Enter(stalker->id, PLAYER_ID, 30.0f);
	caught.isTrigger = true;
	Apply(scene, { throughZone, caught });

	Check(ImpactCount(scene) == 0, "a fast trigger contact is silent");
	CheckNear(scene->player->health, before - stalker->baseDamage, 0.001f,
		"but the Stalker still hurts through a trigger contact");
	delete scene;
}

int main()
{
	// An object binds a model when it enters the map, which needs a GL context.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-contact-rules-tests");

	std::printf("Contact rules tests\n\n");

	TestAProjectileHurtsThePlayerAndIsSpent();
	TestAProjectileHurtsAnEntityFromEitherSide();
	TestAProjectileStrikingTheWorldHurtsNothing();
	TestTwoProjectilesDoNotHurtEachOther();

	TestTheStalkerHurtsThePlayerFromEitherSide();
	TestOnlyTheStalkerHurtsOnContact();

	TestOnlyTheStartOfAContactCounts();
	TestContactsWithMissingBodiesAreSkipped();

	TestImpactLoudnessFollowsClosingSpeed();
	TestImpactIsCreditedToTheFasterBody();
	TestATriggerIsPassedThroughSilently();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
