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

	// Follows the mouse, sits the camera at the head of `body`, and turns `body`
	// to face where the camera looks
	void UpdateCameraFPS(Camera3D* camera, Physics3D::RigidBody3D& body);

};

struct Player : Entity
{
	Player() { kind = ENTITYKIND_PLAYER; }

	std::unique_ptr<Entity> clone() const override { return std::make_unique<Player>(*this); }

	// Game Mechanics
	Vector2 moveDirection = {};
	PlayerCamera camera = {};
	GameObject* artifact = nullptr;

	// The ids of the items the player picked up. Ids, not pointers, so an item
	// destroyed later leaves an id that finds nothing rather than a dangling
	// pointer. Scene::ClearWorld empties it, since the next world reuses ids.
	std::vector<std::uint64_t> inventory;

	int artifactMode = 0;
	int artifactUnlocked = -1;

	// Status
	int baseJumpPower = 20;
	float currentJumpPower = 20;

	// Time until the next footstep noise. Footsteps are the player's main
	// emission into the SoundField, so gait is the primary stealth control.
	float footstepTimer = 0.0f;

	// Render And Update
	void render2D() override; // The reticle; the Scene draws it only while gameplay has input
	void render3D() override;
	void onEnable() override;
	void onDisable() override;
	void update3D(SceneManagement::Scene& scene, float deltaTime);

	// Audio
	FMOD_3D_ATTRIBUTES getListener();

	// Status Functions
	std::ranlux24_base rng;

	// Combat Functions
	void Interact(SceneManagement::Scene& scene);

private:
	int defaultInteractRange = 5;
	int extendedInteractRange = 10;
};

#endif
