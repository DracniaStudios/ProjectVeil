#pragma once
#ifndef ENTITY_H
#define ENTITY_H

#include <GameObject.h>

struct SceneManager;
struct Scene;

struct Entity : public GameObject
{
private:
	float attackTime = 1;
public:

	/** Status **/
	float stamina = 1.0f;

	float baseDamage = 1.0f;
	float baseSpeed = 1.0f;

	/** Functions **/
	virtual void onEnable() override;
	virtual void onDisable() override;
	virtual void render3D() override;
	virtual void update(Scene* scene, float deltaTime) override;

	/** Statuss **/
	virtual float getMaxStamina() { return 100; }

	/** Combat Functions **/
	virtual void onHit(const GameObject* collider) override;
	virtual void Attack();

};

#endif
