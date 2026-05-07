#pragma once
#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <raylib.h>
#include <Physics.h>


struct GameObject 
{
	bool isEnabled = true;
	bool displayDirection = true;
	bool display3DModel = true;
	bool displayCollider = false;
	int id = 0;
	
	/// Physics
	RigidBody3D rigidBody3D;

	/// Renderer
	Model model;
	Mesh mesh;
	Color defaultColor = BLUE;

	Vector3 getPosition() { return rigidBody3D.translation; }
	Quaternion getRotation() { return rigidBody3D.rotation; }
	Vector3 getSize() { return rigidBody3D.scale; }

	virtual void update(float deltaTime);
	virtual void render2D();
	virtual void render3D();
	virtual void onEnable();
	virtual void onDisable();

	/// Add a child to the GameObject
	/// Add Component to the GameObject
	/// Add a Rigidbody to the GameObject if enabled

};

#endif
