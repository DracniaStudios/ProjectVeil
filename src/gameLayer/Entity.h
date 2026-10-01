#pragma once
#ifndef ENTITY_H
#define ENTITY_H

#include <GameObject.h>

#include <memory>

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

/**
 * Concrete Entity type, persisted so a subclass survives a save/load round trip.
 */
enum EntityKind : std::uint8_t
{
	ENTITYKIND_NONE,
	ENTITYKIND_PLAYER,
	ENTITYKIND_STALKER,
	ENTITYKIND_COUNT
};

inline const char* entityKindToString(int kind)
{
	switch (kind)
	{
	case ENTITYKIND_PLAYER:  return "Player";
	case ENTITYKIND_STALKER: return "Stalker";
	default:                 return "Entity";
	}
}

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
	Vector3 spawnPoint = Vector3(0, 0, 0);
public:
	Entity();

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

	/** Identity **/
	// Which concrete class this is. Set by each subclass's constructor and
	// round-tripped through JSON so the loader can rebuild the right type.
	EntityKind kind = ENTITYKIND_NONE;

	/**
	 * Polymorphic copy.
	 */
	virtual std::unique_ptr<Entity> clone() const { return std::make_unique<Entity>(*this); }

	// Builds an empty instance of the requested kind for the save loader, which
	// has to construct the object before it can call loadFromJson() on it.
	static std::unique_ptr<Entity> createByKind(EntityKind kind);

	/** Functions **/
	virtual void onEnable() override;
	virtual void onDisable() override;
	virtual void render3D() override;
	virtual void update(Scene* scene, float deltaTime) override;

	Vector3 getSpawnPoint() { return spawnPoint; }
	void setSpawnPoint(Vector3 spawn) { spawnPoint = spawn; }

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

#endif
