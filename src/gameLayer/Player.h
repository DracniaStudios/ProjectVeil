#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>
#include <optional>
#include <Entity.h>
#include <randomStuff.h>
#include <sim/PlayerCommand.h>

struct PlayerCamera : Camera3D
{
	Vector3 forward = {};
	Vector3 back = {};
	Vector3 right = {};
	Vector3 left = {};
	Vector3 up = {};
	Vector3 down = {};
	Vector3 offset = {};

	// The unit direction a yaw and pitch look along. Yaw 0 looks down +Z.
	static Vector3 LookDirection(float yaw, float pitch);

	// Simulation: the look basis (forward through down) for these angles, with
	// `body` turned to face them on the ground plane
	void SetLook(float yaw, float pitch, Physics3D::RigidBody3D& body);

	// Client: puts a camera at `eye`, looking along (yaw, pitch)
	static void PlaceCamera(Camera3D* camera, Vector3 eye, float yaw, float pitch);
};

struct Player : Entity
{
	Player() { kind = ENTITYKIND_PLAYER; }

	std::unique_ptr<Entity> clone() const override { return std::make_unique<Player>(*this); }

	// Game Mechanics
	Vector2 moveDirection = {};
	PlayerCamera camera = {};
	// The artifact this player carries, once an Unlock pickup has given one. Owned
	// by value: it is not a world object (no id, not saved, never collides), so it
	// never enters the GameMap.
	std::optional<GameObject> artifact;

	int artifactMode = 0;
	int artifactUnlocked = -1;

	// Status
	int baseJumpPower = 20;
	float currentJumpPower = 20;

	// Time until the next footstep noise. Footsteps are the player's main
	// emission into the SoundField, so gait is the primary stealth control.
	float footstepTimer = 0.0f;

	// Where the player looks, in radians. Turned only by the commands a tick
	// applies; yaw 0 looks down +Z, and pitch stays within +/- kPitchLimit.
	float lookYaw = 0.0f;
	float lookPitch = 0.0f;
	static constexpr float kPitchLimit = 1.5f;

	// The player's own flashlight, switched by BUTTON_FLASHLIGHT. Lighting draws
	// the local player's; a Snapshot carries every player's.
	bool flashlightOn = false;

	// Update
	void onEnable() override;
	void onDisable() override;

	// One simulation tick of the player, driven by that tick's command. Reads no
	// input device: everything the player does arrives in `command`.
	void update3D(SceneManagement::Scene& scene, const Sim::PlayerCommand& command, float deltaTime);

	// Turns the look by a command's deltas, clamped as the server clamps them
	// (rule P-6), and points the look basis and the body along it
	void ApplyLook(float yawDelta, float pitchDelta);

	// Puts the look at these angles directly, for spawning and a Full wipe
	void SetLookAngles(float yaw, float pitch);

	// The point the player sees from: the top of the body, lower while crouching
	Vector3 EyePosition() const;

	// Status Functions
	std::ranlux24_base rng;

	// Combat Functions
	void Interact(SceneManagement::Scene& scene);

private:
	int defaultInteractRange = 5;
	int extendedInteractRange = 10;
};

#endif
