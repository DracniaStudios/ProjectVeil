#pragma once
#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <raylib.h>
#include <Physics.h>

enum MeshType
{
	MESH_CUSTOM = 0,
	MESH_POLY,
	MESH_PLANE,
	MESH_CUBE,
	MESH_SPHERE,
	MESH_HEMISPHERE,
	MESH_CYLINDER,
	MESH_CONE,
	MESH_COUNT
};
struct GameObject 
{
	const char* name = "GameObject";

	int id = 0;
	bool isEnabled = true;
	bool canBeSelected = true;
	bool displayDirection = true;
	bool display3DModel = true;
	bool displayCollider = false;
	
	int health = 1;

	/// Physics
	RigidBody3D rigidBody3D = {};
	RigidBody2D rigidBody2D = {};

	Color defaultColor = BLUE;

	/// Renderer
	Model model = {};
	Mesh mesh = {};
	Vector4 meshData = Vector4One();
	int meshVariant = MESH_CUBE;

	Vector3 getPosition() { return rigidBody3D.translation; }
	Quaternion getRotation() { return rigidBody3D.rotation; }
	Vector3 getSize() { return rigidBody3D.scale; }

	virtual void update(GameMap gameMap, float deltaTime);
	virtual void render2D();
	virtual void render3D();
	virtual void onEnable();
	virtual void onDisable();
	virtual int getMaxHealth() { return 10; }
	/// Add a child to the GameObject
	/// Add Component to the GameObject
	/// Add a Rigidbody to the GameObject if enabled

};

#endif
