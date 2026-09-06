#include "Entity.h"

#include <complex>
#include <SceneManager.h>
#include <helpers.h>
#include <Player.h>
#include <AI/Stalker.h>
// Length in milliseconds

// One timer per Buff, keyed by cooldownID — what getBuff() looks up.
static void InstallDefaultBuffs(std::vector<CooldownTimer>& buffTimers)
{
	buffTimers.clear();

	// Change Manually when needed
	for (int i = 0; i < MAX_BUFF; i++) {
		auto buff = CooldownTimer(5); // Default Buff Time
		buff.cooldownID = i;
		buffTimers.push_back(buff);
	}
}

std::unique_ptr<Entity> Entity::createByKind(EntityKind kind)
{
	switch (kind)
	{
	case ENTITYKIND_PLAYER:  return std::make_unique<Player>();
	case ENTITYKIND_STALKER: return std::make_unique<Stalker>();
	case ENTITYKIND_NONE:    return std::make_unique<Entity>();
	default:
		// An unknown kind means a save written by a newer build. Fall back to a
		// plain Entity rather than dropping the row: position and health still
		// load, and the object stays visible instead of vanishing from the world.
		std::cerr << "[Entity] Unknown EntityKind " << static_cast<int>(kind)
		          << ", loading as base Entity\n";
		return std::make_unique<Entity>();
	}
}

Entity::Entity() {
	type = OBJECT_ENTITY;

	// Installed here as well as in onEnable() because the save loaders build
	// entities through loadFromJson and never call onEnable — those entities
	// came up with an empty buffTimers vector, so every getBuff() on them
	// returned nullptr (no buff ever applied) while logging a miss to stdout
	// once per lookup per frame. Doing it in onEnable alone is not enough, and
	// making the loaders call onEnable instead would reset loaded health and
	// stamina back to maximum.
	InstallDefaultBuffs(buffTimers);
}

void Entity::onEnable()
{
	health = getMaxHealth();
	stamina = getMaxStamina();

	// Reset Buffs
	InstallDefaultBuffs(buffTimers);

	// Spawn Entity
	rigidBody3D.SetVelocity(Vector3Zero());

	GameObject::onEnable();
}

void Entity::onDisable()
{
	GameObject::onDisable();
}

void Entity::render3D()
{
	GameObject::render3D();
}

void Entity::update(Scene* scene, float deltaTime)
{
	GameObject::update(scene, deltaTime);

#pragma region Status Effects
	// Check Stamina
	currentSpeed = isSprinting && stamina >= 1 ? baseSpeed * 2 :
		isCrouching ? baseSpeed * 0.5f : baseSpeed;

	if (auto* movementBuff = getBuff(BUFF_MOVEMENT);
		movementBuff && movementBuff->remaining_time() > 0) {
		currentSpeed *= 2;
	}
	
	// Per-second rates. These were applied per FRAME, which made the whole
	// stamina economy a function of the player's refresh rate: sprinting drained
	// a full bar in 3.3s at 60 FPS and in 1.4s at 144. The constants below are
	// the old per-frame amounts multiplied up by the 60 FPS the game targets by
	// default, so the tuning that was authored is preserved.
	constexpr float kSprintDrainPerSecond = 0.5f * 60.0f;
	constexpr float kStaminaRegenPerSecond = 0.01f * 60.0f;

	if (isSprinting) {
		stamina -= kSprintDrainPerSecond * deltaTime;
	}
	else{
		stamina += kStaminaRegenPerSecond * deltaTime;
	}
	stamina = Clamp(stamina, 0, getMaxStamina());
	health = Clamp(health, 0, getMaxHealth());

#pragma endregion
	// isFiring reflects the fire key each frame (see Player.cpp UpdateActions);
	// forceFire is a debug-only override exposed in the World Editor inspectors.
	// Neither used to reach Attack() with a cooldown, so normal play could never
	// fire at all, and forceFire spawned a projectile every single frame.
	canAttack = attackCooldown.is_ready();
	if ((isFiring || forceFire) && canAttack) { Attack(); }

}

/** Combat Functions **/

// Damage reactions live here (virtual dispatch) instead of in the physics
// solver so only real Entity instances ever take damage

void Entity::onCollision(const GameObject* collider)
{
	if (collider->type == OBJECT_PROJECTILE)
	{
		applyHealthValue(collider->baseDamage, true);
	}
}
// Move OnHit to OnCollision
void Entity::onHit(const Entity* collider)
{
	if (collider->type == OBJECT_PROJECTILE || collider->type == OBJECT_ENTITY) {
		health -= collider->baseDamage;
		std::cout << "Entity hit! Health: " << health << " @ Entity.cpp \n";
	}
}

void Entity::Attack()
{
	attackCooldown.use();
	Entity projectile = {};
	projectile.name = "Projectile";
	projectile.type = OBJECT_PROJECTILE;
	projectile.baseDamage = baseDamage;
	projectile.baseSpeed = 10;
	
	projectile.rigidBody3D.Teleport(rigidBody3D.translation + rigidBody3D.forward);
	projectile.rigidBody3D.scale = Vector3(0.2f, 0.2f, 0.2f);
	// No GenMeshSphere here. render3D() draws `model`, never `mesh`, so the
	// generated sphere was never visible — and the projectile is copied into
	// GameMap::gameObjects (slicing to GameObject) and later erased by the
	// pendingDestroy sweep, which never runs onDisable(), so nothing ever called
	// UnloadMesh on it. Every shot leaked its vertex buffers, once per frame for
	// as long as forceFire was held.

	projectile.rigidBody3D.SetVelocity(Vector3Scale(rigidBody3D.forward, projectile.baseSpeed * 10));

	projectile.health = -1;
	projectile.isAlive = false;

	// onEnable() (run inside SpawnGameObject()) resets deathSpan, so Decay() must
	// be applied to the saved copy afterward for the 3-second lifetime to stick.
	GameObject* saved = SceneManager::getInstance().currentScene->gameMap.SpawnGameObject(projectile);
	saved->Decay(3);
}

/** Save Data **/

Json Entity::formatToJson()
{
	Json j = GameObject::formatToJson();

	j["Kind"] = static_cast<int>(kind);
	j["Health"] = health;
	j["MaxHealth"] = maxHealth;
	j["Stamina"] = stamina;
	j["MaxStamina"] = maxStamina;
	j["BaseDamage"] = baseDamage;
	j["BaseSpeed"] = baseSpeed;
	j["SpawnX"] = spawnPoint.x;
	j["SpawnY"] = spawnPoint.y;
	j["SpawnZ"] = spawnPoint.z;

	return j;
}

bool Entity::loadFromJson(Json& j)
{
	if (!GameObject::loadFromJson(j)) { return false; }

	// "Kind" is deliberately not read back here. Each constructor sets `kind` to
	// match the class it actually built, so loading the field could only ever
	// make `kind` disagree with the real type — the loader already consumed it
	// through createByKind() to decide which constructor to run.

	maxHealth = j.value("MaxHealth", maxHealth);
	maxStamina = j.value("MaxStamina", maxStamina);
	health = j.value("Health", maxHealth);
	stamina = j.value("Stamina", maxStamina);
	baseDamage = j.value("BaseDamage", baseDamage);
	baseSpeed = j.value("BaseSpeed", baseSpeed);
	spawnPoint.x = j.value("SpawnX", 0.0f);
	spawnPoint.y = j.value("SpawnY", 5.0f);
	spawnPoint.z = j.value("SpawnZ", 0.0f);

	return true;
}

CooldownTimer* Entity::getBuff(int id) {
	for (auto& buff : buffTimers) {
		if (buff.cooldownID == id) {
			return &buff;
		}
	}
	std::cout << "[Entity.cpp] No Buff With ID: " << id << "\n";
	return nullptr;
}