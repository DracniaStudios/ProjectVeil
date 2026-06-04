#include "Entity.h"

#include <complex>
#include <SceneManager.h>
#include <helpers.h>
// Length in milliseconds

void Entity::onEnable()
{
	GameObject::onEnable();

	health = getMaxHealth();
	stamina = getMaxStamina();
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

	stamina = Clamp(stamina, 0, getMaxStamina());

	if (forceFire) { isFiring = true; Attack(); }

}

/** Combat Functions **/

void Entity::onHit(const Entity* collider)
{
	if (collider->type == OBJECT_PROJECTILE || collider->type == OBJECT_ENTITY) {
		auto entity = static_cast<const Entity&>(*collider);
		health -= entity.baseDamage;
		std::cout << "Entity hit! Health: " << health << " @ Entity.cpp \n";
	}
}

void Entity::Attack()
{
	std::cout << "Start Attack: " << name << "\n";

	if (!WaitTimer(attackStartTime, canAttack, 3)) return;

	canAttack = false;
	Entity projectile = {};
	projectile.name = "Projectile";
	projectile.type = OBJECT_PROJECTILE;
	projectile.baseDamage = baseDamage;
	projectile.baseSpeed = 10;
	
	projectile.rigidBody3D.translation = rigidBody3D.translation + rigidBody3D.forward;
	projectile.rigidBody3D.scale = Vector3(0.2f, 0.2f, 0.2f);
	projectile.meshVariant = MESH_CUBE;

	projectile.rigidBody3D.velocity = Vector3Scale(rigidBody3D.forward, projectile.baseSpeed * 10);
	
	projectile.Decay(3);
	projectile.health = -1;
	projectile.isAlive = false;

	SceneManager::getInstance().currentScene->gameMap.saveObjectAt(projectile.rigidBody3D.translation, projectile);
}
