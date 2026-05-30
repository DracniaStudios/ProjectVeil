#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>

#include <GameObject.h>

struct Player;
struct SceneManager;
struct PlayerCamera;

struct PlayerCamera
{
	Vector3 forward;
	Vector2 sensitivity = Vector2{ 0.01f, 0.01f };
	Vector2 lookRotation = Vector2{ 0, 0 };
	Vector2 lean = Vector2{ 0, 0 };
	float headLerp;
	float walkLerp;
	float headTimer;


	void UpdateCameraFPS(Camera* camera, Player* player);

};

struct Player : GameObject
{
	Vector2 moveDirection = {};
	bool isCrouching = false;

	float baseSpeed = 1;
	float stamina = 0;

	/// Camera Data
	PlayerCamera camera = {};

	void render2D() override;
	void render3D() override;
	void onEnable() override;
	void onDisable() override;
	void update2D(SceneManager* manager, float deltaTime, bool canMove = true);
	void update3D(SceneManager* manager, float deltaTime);
	int getMaxHealth() override { return 20; }
	int getMaxStamina() { return 100; }
};

#endif
