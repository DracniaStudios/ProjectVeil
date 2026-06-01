#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>

#include <Entity.h>

struct SceneManager;
struct Scene;
struct Player;
struct PlayerCamera;

struct PlayerCamera
{
	Vector3 forward;
	Vector3 offset;
	Vector3 position;
	Vector2 sensitivity = Vector2{ 0.01f, 0.01f };
	Vector2 lookRotation = Vector2{ 0, 0 };
	Vector2 lean = Vector2{ 0, 0 };
	float headLerp;
	float walkLerp;
	float headTimer;


	void UpdateCameraFPS(Camera* camera, Player* player);

};

struct Player : public Entity
{
private:
	bool isFiring = false;

public:

	RigidBody2D rigidBody2D = {};

	Vector2 moveDirection = {};

	bool isCrouching = false;

	/// Camera Data
	PlayerCamera camera = {};

	/// Functions
	bool* IsFiring() { return &isFiring; }

	// Render And Update
	void render2D(Scene* scene);
	void render3D(Scene* scene);
	void onEnable() override;
	void onDisable() override;
	void update2D(SceneManager* manager, float deltaTime, bool canMove = true);
	void update3D(SceneManager* manager, float deltaTime);
	
	// Status Functions
	float getMaxHealth() override { return 20; }
	float getMaxStamina() override { return 100; }

	// Combat Functions
	void Fire(Scene* scene);
	void FireLaser(Scene* scene);

};

#endif
