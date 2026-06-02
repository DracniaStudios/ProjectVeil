#include "Entity.h"

#include <complex>
#include <SceneManager.h>

void Entity::onEnable()
{
	GameObject::onEnable();
	name = "Entity";
	type = OBJECT_ENTITY;

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

}

/** Combat Functions **/

void Entity::onHit(const GameObject* collider)
{
	if (collider->type == OBJECT_PROJECTILE || collider->type == OBJECT_ENTITY) {
		Entity* entity = (Entity*)collider;
		health -= entity->baseDamage;
	}
	std::cout << name << " was hit! \n";
}

void Entity::Attack()
{
	Entity* projectile = new Entity();
	projectile->name = "Projectile";
	projectile->type = OBJECT_PROJECTILE;
	projectile->baseDamage = baseDamage * 0.5f;
	projectile->baseSpeed = 10;
	
	projectile->rigidBody3D->translation = rigidBody3D->translation + rigidBody3D->forward;
	projectile->rigidBody3D->scale = Vector3(0.2f, 0.2f, 0.2f);
	projectile->meshVariant = MESH_CUBE;

	projectile->rigidBody3D->velocity = Vector3Scale(rigidBody3D->forward, projectile->baseSpeed * 10);
	
	projectile->Decay(3);
	projectile->health = -1;
	projectile->isAlive = false;

	GameObject* obj = projectile;
	SceneManager::getInstance().currentScene->gameMap.saveObjectAt(projectile->rigidBody3D->translation, *obj);
}
