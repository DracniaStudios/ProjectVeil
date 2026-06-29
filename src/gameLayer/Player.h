#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>
#include <Entity.h>

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


	void UpdateCameraFPS(Camera* camera);

};

struct Player : Entity
{
	RigidBody2D rigidBody2D = {};
	Vector2 moveDirection = {};
	
	// Flags
	bool isCrouching = false;

	// Render And Update
	void render2D() override;
	void render3D() override;
	void onEnable() override;
	void onDisable() override;
	void update2D(float deltaTime, bool canMove = true);
	void update3D(float deltaTime);

	Vector2 getPosition2D() const { return Vector2{ rigidBody2D.translation.x, rigidBody2D.translation.y }; }
	Vector2 getSize2D() const { return Vector2{ rigidBody2D.scale.x, rigidBody2D.scale.y }; }
	// Status Functions

	// Combat Functions
	static void Fire();
	void FireLaser();
	static void Interact();
};

#endif
