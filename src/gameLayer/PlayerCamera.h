#pragma once
#ifndef PLAYERCAMERA_H
#define PLAYERCAMERA_H

#include <raylib.h>

struct Player;

struct PlayerCamera
{
	Vector2 sensitivity = Vector2{ 0.01f, 0.01f };
	Vector2 lookRotation = Vector2{0, 0};
	Vector2 lean = Vector2{ 0, 0};
	float headLerp;
	float walkLerp;
	float headTimer;

	void UpdateCameraFPS(Camera* camera, Player* player);

};

#endif