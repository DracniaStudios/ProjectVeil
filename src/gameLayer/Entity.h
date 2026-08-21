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

enum EntityKind : uint8_t 
{
	ENTITYKIND_NONE,
	ENTITYKIND_PLAYER,
	ENTITYKIND_STALKER
};

/*
struct Buff {
	CooldownTimer timer = {};
	BuffType type = {};
};
*/

inline const char* buffTypeToString(int type) {
	switch (type)
	{
	case BUFF_MOVEMENT:      return "Movement";
	case BUFF_RANGE:		 return "Range";
	case BUFF_HEALTH:        return "Health";
	case BUFF_HEARING:		 return "Hearing";
	case BUFF_SEARCH:		 return "Search";
	case BUFF_RANDOM:		 return "Random";
	default:                 return "None";
	}
}

struct Entity : GameObject
{
private:
	// Gates Attack() so holding the fire key doesn't spawn a projectile every
	// frame. Replaces attackStartTime/attackWaitTime, which were declared for
	// this but never actually wired into update().
	CooldownTimer attackCooldown = CooldownTimer(3.0);
public:
	Entity();
	virtual std::unique_ptr<Entity> clone() const { return std::make_unique<Entity>(*this); };
	EntityKind kind = ENTITYKIND_NONE;

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
	
	CooldownTimer* getBuff(int id);
	float getMaxHealth() const { return maxHealth; }
	float getMaxStamina() const { return maxStamina; }

	/** Save Data **/
	Json formatToJson() override;
	bool loadFromJson(Json& j) override;

	/** Combat Functions **/
	virtual void onHit(const Entity* collider);

	void applyHealthValue(const float& value, bool isDamage){ if (isDamage) { health -= value; } else { health += value; } }
	virtual void Attack();

};

inline Entity createByKind(EntityKind kind) {
	auto entity = Entity{};
	entity.kind = kind;

	if (kind == ENTITYKIND_PLAYER) {
		entity.name = "Player";
		std::cout << "[Entity.cpp] Create Player \n";
	}
	else if (kind == ENTITYKIND_STALKER) {
		entity.name = "Stalker";
		entity.baseDamage = 10;
		std::cout << "[Entity.cpp] Create Stalker \n";
	}
	else {
		entity.name = "New Entity";
		std::cout << "[Entity.cpp] Create Entity Blank \n";
	}

	return entity;
}

#endif
