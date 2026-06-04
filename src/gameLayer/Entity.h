#pragma once
#ifndef ENTITY_H
#define ENTITY_H

#include <iostream>
#include <raylib.h>
#include <Physics.h>

struct Scene;

enum EntityType
{
	ENTITY_FRIENDLY,
	ENTITY_ENEMY,
	ENTITY_PROJECTILE,
	ENTITIY_PLAYER,
	ENTITY_COUNT
};

struct Entity
{
private:
	float attackStartTime = 1.0f;
	float attackWaitTime = 3.0f;
public:

	/** Status **/
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
	float getMaxStamina() const { return maxStamina; }

	/** Combat Functions **/
	virtual void onHit(const Entity* collider);
	void takeDamage(const float& damage)
	{
		std::cout << "Damage Taken: " << damage << "\n";
		health -= damage;
	}
	virtual void Attack();

};

#endif
