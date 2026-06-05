#pragma once
#ifndef ENTITY_H
#define ENTITY_H

#include <iostream>
#include <GameObject.h>

struct Scene;

struct Entity : public GameObject
{
private:
	float attackStartTime = 1.0f;
	float attackWaitTime = 3.0f;
public:
	
	/** Data **/
	const char* name = "Entity";
	
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

	virtual void onEnable();
	virtual void onDisable();
	virtual void render3D();
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
