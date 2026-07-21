#include "Entity.h"

#include <complex>
#include <SceneManager.h>
#include <helpers.h>
// Length in milliseconds

void Entity::onEnable()
{
	health = getMaxHealth();
	stamina = getMaxStamina();

	// Reset Buffs
	buffTimers.clear();
	// All Buffs Last 30 seconds
	// Change Manually when needed
	for (int i = 0; i < MAX_BUFF; i++) {
		auto buff = CooldownTimer(5);
		buff.cooldownID = i;
		buffTimers.push_back(buff);
	}
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
	currentSpeed = isSprinting ? baseSpeed * 2 :
		isCrouching ? baseSpeed * 0.5f : baseSpeed;

	if (auto* movementBuff = getBuff(BUFF_MOVEMENT);
		movementBuff && movementBuff->remaining_time() > 0) {
		currentSpeed *= 2;
	}
	
	if (isSprinting) stamina -= 0.5f;
	stamina = Clamp(stamina, 0, getMaxStamina());
	
	health = Clamp(health, 0, getMaxHealth());

#pragma endregion
	if (forceFire) { Attack(); }

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

void Entity::onHit(const Entity* collider)
{
	if (collider->type == OBJECT_PROJECTILE || collider->type == OBJECT_ENTITY) {
		health -= collider->baseDamage;
		std::cout << "Entity hit! Health: " << health << " @ Entity.cpp \n";
	}
}

void Entity::Attack()
{
	canAttack = false;
	Entity projectile = {};
	projectile.name = "Projectile";
	projectile.type = OBJECT_PROJECTILE;
	projectile.baseDamage = baseDamage;
	projectile.baseSpeed = 10;
	
	projectile.rigidBody3D.Teleport(rigidBody3D.translation + rigidBody3D.forward);
	projectile.rigidBody3D.scale = Vector3(0.2f, 0.2f, 0.2f);
	projectile.mesh = GenMeshSphere(0.2f, 16, 16);

	projectile.rigidBody3D.SetVelocity(Vector3Scale(rigidBody3D.forward, projectile.baseSpeed * 10));
	
	projectile.Decay(3);
	projectile.health = -1;
	projectile.isAlive = false;

	SceneManager::getInstance().currentScene->gameMap.saveObject(projectile);
}

/** Save Data **/

Json Entity::formatToJson()
{
	Json j = GameObject::formatToJson();

	j["Health"] = health;
	j["MaxHealth"] = maxHealth;
	j["Stamina"] = stamina;
	j["MaxStamina"] = maxStamina;
	j["BaseDamage"] = baseDamage;
	j["BaseSpeed"] = baseSpeed;

	return j;
}

bool Entity::loadFromJson(Json& j)
{
	if (!GameObject::loadFromJson(j)) { return false; }

	maxHealth = j.value("MaxHealth", maxHealth);
	maxStamina = j.value("MaxStamina", maxStamina);
	health = j.value("Health", maxHealth);
	stamina = j.value("Stamina", maxStamina);
	baseDamage = j.value("BaseDamage", baseDamage);
	baseSpeed = j.value("BaseSpeed", baseSpeed);

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