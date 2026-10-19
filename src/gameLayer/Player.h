#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>
#include <Entity.h>
#include <randomStuff.h>

struct PlayerCamera : Camera3D
{
	Vector3 forward = {};
	Vector3 back = {};
	Vector3 right = {};
	Vector3 left = {};
	Vector3 up = {};
	Vector3 down = {};
	Vector3 offset = {};
	Vector2 sensitivity = Vector2{ 0.01f, 0.01f };
	Vector2 lookRotation = Vector2{ 0, 0 };
	Vector2 lean = Vector2{ 0, 0 };
	float headLerp = {};
	float walkLerp = {};
	float headTimer = {};

	void UpdateCameraFPS(Camera3D* camera);

};

struct Player : Entity
{
	Player() { kind = ENTITYKIND_PLAYER; }

	std::unique_ptr<Entity> clone() const override { return std::make_unique<Player>(*this); }

	// Game Mechanics
	Vector2 moveDirection = {};
	PlayerCamera camera = {};
	GameObject* artifact = nullptr;

	std::vector<InteractableObject*> inventory;// Set TO Object IDs if doesn't work (just like ActivatorObject).

	int artifactMode = 0;
	int artifactUnlocked = -1;

	// Status
	int baseJumpPower = 20;
	float currentJumpPower = 20;

	// Time until the next footstep noise. Footsteps are the player's main
	// emission into the SoundField, so gait is the primary stealth control.
	float footstepTimer = 0.0f;

	// Render And Update
	void render2D() override;
	void render3D() override;
	void onEnable() override;
	void onDisable() override;
	void update3D(float deltaTime);

	// Audio
	FMOD_3D_ATTRIBUTES getListener();

	// Status Functions
	std::ranlux24_base rng;

	// Combat Functions
	void Interact();

private:
	int defaultInteractRange = 5;
	int extendedInteractRange = 10;
};

#endif
