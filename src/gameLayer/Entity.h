#pragma once
#ifndef ENTITY_H
#define ENTITY_H

#include <GameObject.h>

struct Scene;

struct Entity : GameObject
{
private:
	float attackStartTime = 1.0f;
	float attackWaitTime = 3.0f;
public:
	/** Status **/
	float health = 1.0f;
	float maxHealth = 10.0f;

	float stamina = 1.0f;
	float maxStamina = 100.0f;

	float baseDamage = 1.0f;
	float baseSpeed = 1.0f;

	/** Flags **/
	bool isFiring = false;
	bool forceFire = false;
	bool canAttack = true;

	/** Functions **/

	virtual void onEnable() override;
	virtual void onDisable() override;
	virtual void render3D() override;
	virtual void update(Scene* scene, float deltaTime) override;

	/** Statuss **/
	float getMaxHealth() const { return maxHealth; }
	float getMaxStamina() const { return maxStamina; }

	/** Combat Functions **/
	virtual void onHit(const Entity* collider);
	void takeDamage(const float& damage)
	{
		health -= damage;
	}
	virtual void Attack();

};

#endif
