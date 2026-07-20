#pragma once
#ifndef ENTITY_H
#define ENTITY_H

#include <GameObject.h>

struct Scene;

enum Buff {
	BUFF_MOVEMENT,
	BUFF_RANGE,
	BUFF_HEALTH,
	BUFF_HEARING,
	BUFF_SEARCH,
	BUFF_RANDOM,
	// Attack Timer?
	MAX_BUFF
};

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

	float baseSpeed = 1.0f;
	float currentSpeed = 1.0f;

	/** Flags **/

	bool isSprinting = false;
	bool isCrouching = false;
	bool isFiring = false;
	bool forceFire = false;
	bool canAttack = true;

	/** Functions **/
	virtual void onEnable() override;
	virtual void onDisable() override;
	virtual void render3D() override;
	virtual void update(Scene* scene, float deltaTime) override;
	virtual void onCollision(const GameObject* collider) override;

	/** Status **/
	std::vector<CooldownTimer> buffTimers = {};
	
	bool useBuff(int id);
	CooldownTimer* getBuff(int id);
	float getMaxHealth() const { return maxHealth; }
	float getMaxStamina() const { return maxStamina; }


	/** Save Data **/
	Json formatToJson() override;
	bool loadFromJson(Json& j) override;

	/** Combat Functions **/
	virtual void onHit(const Entity* collider);

	void applyHealthValue(const float& value, bool isDamage){ health = isDamage ? health -= value : health += value; }
	virtual void Attack();

};

#endif
