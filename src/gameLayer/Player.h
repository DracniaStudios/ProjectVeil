#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>
#include <Entity.h>


struct Player;
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
public:

	RigidBody2D rigidBody2D = {};
	Vector2 moveDirection = {};

	bool isFiring = false;
	bool isCrouching = false;

	/// Camera Data
	PlayerCamera camera = {};

	/// Functions

	// Render And Update
	void render2D();
	void render3D();
	void onEnable() override;
	void onDisable() override;
	void update2D(float deltaTime, bool canMove = true);
	void update3D(float deltaTime);
	
	// Status Functions

	// Combat Functions
	void Fire();
	void FireLaser();

};

#endif
