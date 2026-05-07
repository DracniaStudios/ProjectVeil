#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>

#include <GameObject.h>
struct SceneManager;

struct Player : public GameObject
{
	// Player2D is replicated from Player3D, but only for rendering and input purposes.
	
	// The position of Player2D is derived from Player3D's position,
	// and the movement of Player2D is controlled by Player3D's movement.

	// This allows us to have a consistent player representation in both 2D and 3D space,
	// while still maintaining the core logic and physics in the 3D player structure.

	bool isEnabled = true;
	bool displayDirection = true;
	bool display2DModel = true;
	bool display3DModel = true;
	bool displayCollider = false;
	int id = 0;

	/// Physics
	RigidBody3D rigidBody3D;
	RigidBody2D rigidBody2D;

	/// Renderer
	Model model;
	Mesh mesh;
	Color defaultColor = Color(0, 115, 0, 255);

	Vector3 getPosition() { return rigidBody3D.translation; }
	Quaternion getRotation() { return rigidBody3D.rotation; }
	Vector3 getSize() { return rigidBody3D.scale; }


	bool isCrouching = false;

	float baseSpeed = 1;

	void render2D();
	void render3D();
	void onEnable();
	void onDisable();
	void update(SceneManager* manager, float deltaTime);
};

#endif
