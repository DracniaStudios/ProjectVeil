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
	Vector2 sensitivity = Vector2{ 0.01f, 0.01f };
	Vector2 lookRotation = Vector2{ 0, 0 };
	Vector2 lean = Vector2{ 0, 0 };
	float headLerp;
	float walkLerp;
	float headTimer;

	Vector3 forward;

	void UpdateCameraFPS(Camera* camera, Player* player);

};

struct Player :  GameObject
{

	/// Camera Data
	PlayerCamera camera = {};

	bool isCrouching = false;

	float baseSpeed = 1;

	void render2D();
	void render3D();
	void onEnable();
	void onDisable();
	void update(SceneManager* manager, float deltaTime);
};

#endif
