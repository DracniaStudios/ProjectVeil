/**
 * Tests for Entity: the status, combat and save rules every Player and Stalker
 * inherits.
 *
 * An Entity's kind decides which class the save loader rebuilds, its buff
 * timers decide whether a buff is running, its health and stamina are clamped
 * every tick, and Attack() is gated by a cooldown so holding the fire key fires
 * once rather than every frame. Each of these has broken before in a way that
 * only showed up in play (stamina drained by frame rate, loaded entities with no
 * buffs, a projectile per frame), so each has a case here.
 *
 * Like the other engine-linked suites this links against the game's own objects
 * and needs a GL context, since a GameObject binds a model. See
 * tests/run_tests.sh.
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

static void CheckNear(float actual, float expected, float tolerance, const std::string& what)
{
	++g_checks;
	if (std::fabs(actual - expected) > tolerance)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected ~%.4f, got %.4f)\n", what.c_str(), expected, actual);
	}
}

// An enabled entity that stays where it is put
static std::unique_ptr<Entity> MakeEntity()
{
	auto entity = std::make_unique<Entity>();
	entity->rigidBody3D.isStatic = true;
	entity->onEnable(); // full health and stamina
	return entity;
}

static std::vector<GameObject*> Projectiles(SceneManagement::Scene* scene)
{
	std::vector<GameObject*> found;
	scene->gameMap.ForEachGameObject([&](GameObject& object)
	{
		if (object.type == OBJECT_PROJECTILE) { found.push_back(&object); }
	});
	return found;
}

// ─── Kinds ─────────────────────────────────────────────────────────────────

static void TestEachKindBuildsItsOwnClass()
{
	std::printf("the save loader builds the class each kind names\n");

	std::unique_ptr<Entity> player = Entity::createByKind(ENTITYKIND_PLAYER);
	std::unique_ptr<Entity> stalker = Entity::createByKind(ENTITYKIND_STALKER);
	std::unique_ptr<Entity> plain = Entity::createByKind(ENTITYKIND_NONE);

	Check(dynamic_cast<Player*>(player.get()) != nullptr && player->kind == ENTITYKIND_PLAYER, "Player builds a Player");
	Check(dynamic_cast<Stalker*>(stalker.get()) != nullptr && stalker->kind == ENTITYKIND_STALKER, "Stalker builds a Stalker");
	Check(plain != nullptr && plain->kind == ENTITYKIND_NONE, "None builds a plain Entity");
	Check(player->type == OBJECT_ENTITY && stalker->type == OBJECT_ENTITY, "every kind is an entity in the map");
}

static void TestAnUnknownKindStillLoads()
{
	std::printf("a kind from a newer build loads as a plain Entity rather than vanishing\n");

	std::unique_ptr<Entity> unknown = Entity::createByKind(static_cast<EntityKind>(77));
	Check(unknown != nullptr, "something is built");
	Check(unknown != nullptr && unknown->kind == ENTITYKIND_NONE, "and it is a plain Entity");
	Check(dynamic_cast<Player*>(unknown.get()) == nullptr && dynamic_cast<Stalker*>(unknown.get()) == nullptr,
		"not a Player or Stalker by accident");
}

static void TestACloneKeepsItsClassAndState()
{
	std::printf("a clone is the same class as the original, with the same state\n");

	Stalker original;
	original.waypoints = { Vector3{ 1, 0, 1 }, Vector3{ 4, 0, 4 } };
	original.health = 3.5f;
	std::unique_ptr<Entity> copy = original.clone();

	Stalker* stalker = dynamic_cast<Stalker*>(copy.get());
	Check(stalker != nullptr, "a cloned Stalker is a Stalker");
	Check(stalker != nullptr && stalker->waypoints.size() == 2, "with its patrol route");
	Check(copy->health == 3.5f, "and its health");

	Player player;
	Check(dynamic_cast<Player*>(player.clone().get()) != nullptr, "a cloned Player is a Player");
}

static void TestKindAndBuffNames()
{
	std::printf("kinds and buffs have names for the inspector\n");

	Check(std::string(entityKindToString(ENTITYKIND_PLAYER)) == "Player", "Player");
	Check(std::string(entityKindToString(ENTITYKIND_STALKER)) == "Stalker", "Stalker");
	Check(std::string(entityKindToString(ENTITYKIND_NONE)) == "Entity", "a plain entity is Entity");
	Check(std::string(buffTypeToString(BUFF_HEARING)) == "Hearing", "the Hearing buff");
	Check(std::string(buffTypeToString(MAX_BUFF)) == "None", "past the last buff is None");
}

// ─── Buffs ─────────────────────────────────────────────────────────────────

static void TestEveryBuffHasATimer()
{
	std::printf("a new entity has an idle timer for every buff\n");
	Entity entity;

	for (int buff = 0; buff < MAX_BUFF; ++buff)
	{
		CooldownTimer* timer = entity.getBuff(buff);
		Check(timer != nullptr, std::string(buffTypeToString(buff)) + " has a timer");
		if (timer == nullptr) { continue; }
		Check(timer->cooldownID == buff, std::string(buffTypeToString(buff)) + "'s timer is its own");
		Check(timer->remaining_time() == 0.0, std::string(buffTypeToString(buff)) + " is not running");
	}

	Check(entity.getBuff(MAX_BUFF) == nullptr, "there is no buff past the last");
	Check(entity.getBuff(-1) == nullptr, "nor before the first");
}

static void TestALoadedEntityHasItsBuffs()
{
	std::printf("an entity built by the save loader, which never enables it, still has its buffs\n");

	std::unique_ptr<Entity> loaded = Entity::createByKind(ENTITYKIND_STALKER);
	Json row = MakeEntity()->formatToJson();
	loaded->loadFromJson(row);

	Check(loaded->buffTimers.size() == MAX_BUFF, "one timer per buff");
	Check(loaded->getBuff(BUFF_MOVEMENT) != nullptr, "the movement buff can be found");
}

static void TestEnablingResetsRunningBuffs()
{
	std::printf("enabling an entity stops any buff that was running\n");
	std::unique_ptr<Entity> entity = MakeEntity();

	entity->getBuff(BUFF_RANGE)->use();
	Check(entity->getBuff(BUFF_RANGE)->remaining_time() > 0.0, "the buff is running");

	entity->onEnable();
	Check(entity->getBuff(BUFF_RANGE)->remaining_time() == 0.0, "and is idle after onEnable");
}

// ─── Health and stamina ────────────────────────────────────────────────────

static void TestEnablingRestoresHealthAndStamina()
{
	std::printf("enabling an entity restores its health and stamina to their maximums\n");
	Entity entity;
	entity.maxHealth = 25.0f;
	entity.maxStamina = 40.0f;
	entity.health = 1.0f;
	entity.stamina = 0.0f;

	entity.onEnable();
	Check(entity.health == 25.0f, "health is full");
	Check(entity.stamina == 40.0f, "stamina is full");
}

static void TestDamageAndHealing()
{
	std::printf("damage takes health away, healing gives it back\n");
	std::unique_ptr<Entity> entity = MakeEntity();
	const float full = entity->health;

	entity->applyHealthValue(3.0f, true);
	CheckNear(entity->health, full - 3.0f, 1e-5f, "3 damage costs 3 health");
	entity->applyHealthValue(1.5f, false);
	CheckNear(entity->health, full - 1.5f, 1e-5f, "1.5 healing restores 1.5");

	Entity bullet;
	bullet.type = OBJECT_PROJECTILE;
	bullet.baseDamage = 2.0f;
	entity->onHit(&bullet);
	CheckNear(entity->health, full - 3.5f, 1e-5f, "a hit from a projectile costs its damage");

	Entity scenery;
	scenery.type = OBJECT_GENERIC;
	scenery.baseDamage = 50.0f;
	entity->onHit(&scenery);
	CheckNear(entity->health, full - 3.5f, 1e-5f, "a hit from scenery costs nothing");
}

static void TestATickKeepsHealthAndStaminaInRange()
{
	std::printf("a tick clamps health and stamina between zero and their maximums\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	std::unique_ptr<Entity> entity = MakeEntity();

	entity->health = -12.0f;
	entity->stamina = -3.0f;
	entity->update(scene, 1.0f / 60.0f);
	Check(entity->health == 0.0f, "overkill leaves health at 0, not below");
	Check(entity->stamina >= 0.0f, "stamina never goes below 0");

	entity->health = entity->getMaxHealth() + 40.0f;
	entity->stamina = entity->getMaxStamina() + 40.0f;
	entity->update(scene, 1.0f / 60.0f);
	Check(entity->health == entity->getMaxHealth(), "overhealing stops at the maximum");
	Check(entity->stamina == entity->getMaxStamina(), "stamina stops at the maximum");
	delete scene;
}

static void TestSprintDrainIsPerSecondNotPerFrame()
{
	std::printf("sprinting drains the same stamina in a second at any frame rate\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	std::unique_ptr<Entity> at60 = MakeEntity();
	std::unique_ptr<Entity> at144 = MakeEntity();
	at60->isSprinting = true;
	at144->isSprinting = true;

	for (int i = 0; i < 60; ++i) { at60->update(scene, 1.0f / 60.0f); }
	for (int i = 0; i < 144; ++i) { at144->update(scene, 1.0f / 144.0f); }

	CheckNear(at60->stamina, at144->stamina, 0.01f, "60 and 144 FPS end the second with the same stamina");
	CheckNear(at60->stamina, at60->getMaxStamina() - 30.0f, 0.01f, "a second of sprinting costs 30 stamina");

	at60->isSprinting = false;
	const float rested = at60->stamina;
	for (int i = 0; i < 60; ++i) { at60->update(scene, 1.0f / 60.0f); }
	CheckNear(at60->stamina, rested + 0.6f, 0.01f, "a second of rest regains 0.6");
	delete scene;
}

static void TestSpeedFollowsGait()
{
	std::printf("sprinting doubles the speed, crouching halves it, and a movement buff doubles it again\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	std::unique_ptr<Entity> entity = MakeEntity();
	entity->baseSpeed = 4.0f;
	const float dt = 1.0f / 60.0f;

	entity->update(scene, dt);
	Check(entity->currentSpeed == 4.0f, "walking is the base speed");

	entity->isSprinting = true;
	entity->update(scene, dt);
	Check(entity->currentSpeed == 8.0f, "sprinting is twice that");

	entity->stamina = 0.0f;
	entity->update(scene, dt);
	Check(entity->currentSpeed == 4.0f, "with no stamina left, a sprint is a walk");

	entity->isSprinting = false;
	entity->isCrouching = true;
	entity->update(scene, dt);
	Check(entity->currentSpeed == 2.0f, "crouching is half");

	entity->getBuff(BUFF_MOVEMENT)->use();
	entity->update(scene, dt);
	Check(entity->currentSpeed == 4.0f, "a movement buff doubles even a crouch");
	delete scene;
}

// ─── Attacking ─────────────────────────────────────────────────────────────

static void TestHoldingFireFiresOnce()
{
	std::printf("holding fire spawns one projectile, then waits for the cooldown\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	std::unique_ptr<Entity> entity = MakeEntity();
	entity->baseDamage = 2.5f;
	entity->rigidBody3D.Teleport(Vector3{ 3, 1, 3 });
	entity->forceFire = true;

	for (int i = 0; i < 30; ++i) { entity->update(scene, 1.0f / 60.0f); }

	const std::vector<GameObject*> fired = Projectiles(scene);
	Check(fired.size() == 1, "half a second of holding fire spawns one projectile (got " + std::to_string(fired.size()) + ")");
	entity->update(scene, 1.0f / 60.0f);
	Check(!entity->canAttack, "and the entity cannot attack again yet");

	if (!fired.empty())
	{
		GameObject* projectile = fired.front();
		Check(projectile->baseDamage == 2.5f, "the projectile carries the entity's damage");
		Check(projectile->deathSpan == 3.0f, "and decays after three seconds");
		const Vector3 position = projectile->getPosition();
		Check(std::fabs(position.x - 3.0f) < 0.01f && std::fabs(position.y - 1.0f) < 0.01f && std::fabs(position.z - 4.0f) < 0.01f,
			"it starts one unit in front of the entity");
		Check(Vector3Length(projectile->getVelocity()) > 0.0f, "and is moving");
	}
	delete scene;
}

static void TestNoFireNoProjectile()
{
	std::printf("an entity that is not firing spawns nothing\n");
	SceneManagement::Scene* scene = new SceneManagement::Scene();
	std::unique_ptr<Entity> entity = MakeEntity();

	for (int i = 0; i < 30; ++i) { entity->update(scene, 1.0f / 60.0f); }
	Check(Projectiles(scene).empty(), "no projectile");
	Check(entity->canAttack, "and the entity is ready to attack");
	delete scene;
}

// ─── Save data ─────────────────────────────────────────────────────────────

static void TestTheStatusRoundTrips()
{
	std::printf("an entity's status and spawn point round-trip through its save row\n");
	std::unique_ptr<Entity> saved = MakeEntity();
	saved->maxHealth = 30.0f;
	saved->health = 12.5f;
	saved->maxStamina = 80.0f;
	saved->stamina = 41.0f;
	saved->baseDamage = 4.0f;
	saved->baseSpeed = 2.5f;
	saved->setSpawnPoint(Vector3{ -3, 2, 9 });

	Json row = saved->formatToJson();
	Entity loaded;
	Check(loaded.loadFromJson(row), "the row loads");

	Check(loaded.maxHealth == 30.0f && loaded.health == 12.5f, "health and its maximum");
	Check(loaded.maxStamina == 80.0f && loaded.stamina == 41.0f, "stamina and its maximum");
	Check(loaded.baseDamage == 4.0f && loaded.baseSpeed == 2.5f, "damage and speed");
	const Vector3 spawn = loaded.getSpawnPoint();
	Check(spawn.x == -3.0f && spawn.y == 2.0f && spawn.z == 9.0f, "the spawn point");
}

static void TestARowMissingItsStatusLoadsAtFull()
{
	std::printf("a row with no health or stamina loads at full, and spawns above the origin\n");
	std::unique_ptr<Entity> saved = MakeEntity();
	saved->maxHealth = 20.0f;
	saved->maxStamina = 60.0f;

	Json row = saved->formatToJson();
	for (const char* key : { "Health", "Stamina", "SpawnX", "SpawnY", "SpawnZ" }) { row.erase(key); }

	Entity loaded;
	loaded.loadFromJson(row);
	Check(loaded.health == 20.0f, "health is the saved maximum");
	Check(loaded.stamina == 60.0f, "stamina is the saved maximum");
	const Vector3 spawn = loaded.getSpawnPoint();
	Check(spawn.x == 0.0f && spawn.y == 5.0f && spawn.z == 0.0f, "the spawn point is (0, 5, 0)");
}

static void TestLoadingNeverChangesTheKind()
{
	std::printf("loading a row never changes the kind of the class that loads it\n");

	Stalker stalker;
	Json row = stalker.formatToJson();
	Check(row.value("Kind", -1) == ENTITYKIND_STALKER, "a Stalker saves its kind");

	Player player;
	player.loadFromJson(row);
	Check(player.kind == ENTITYKIND_PLAYER, "a Player loading a Stalker's row stays a Player");
}

int main()
{
	// A GameObject binds a model, which needs a GL context.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-entity-tests");

	std::printf("Entity tests\n\n");

	TestEachKindBuildsItsOwnClass();
	TestAnUnknownKindStillLoads();
	TestACloneKeepsItsClassAndState();
	TestKindAndBuffNames();

	TestEveryBuffHasATimer();
	TestALoadedEntityHasItsBuffs();
	TestEnablingResetsRunningBuffs();

	TestEnablingRestoresHealthAndStamina();
	TestDamageAndHealing();
	TestATickKeepsHealthAndStaminaInRange();
	TestSprintDrainIsPerSecondNotPerFrame();
	TestSpeedFollowsGait();

	TestHoldingFireFiresOnce();
	TestNoFireNoProjectile();

	TestTheStatusRoundTrips();
	TestARowMissingItsStatusLoadsAtFull();
	TestLoadingNeverChangesTheKind();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
