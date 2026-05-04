#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>

#include <GameObject.h>

struct Player : public GameObject	
{
	// Player2D is replicated from Player3D, but only for rendering and input purposes.
	
	// The position of Player2D is derived from Player3D's position,
	// and the movement of Player2D is controlled by Player3D's movement.

	// This allows us to have a consistent player representation in both 2D and 3D space,
	// while still maintaining the core logic and physics in the 3D player structure.


	bool isCrouching = false;

	Color defaultColor = Color(0, 115, 0, 255);

	float baseSpeed = 0.01f;

	void render2D() override;
	void render3D() override;
	//void onEnable() override;
	//void onDisable() override;
	void update(Camera* camera, float deltaTime);
};

#endif
