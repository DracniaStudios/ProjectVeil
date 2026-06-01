#include "Entity.h"

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
