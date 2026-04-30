#pragma once
#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <raylib.h>



struct GameObject {

	int id = 0;
	Transform transform;

	Vector3 getPosition() { return transform.translation; }
	Quaternion getRotation() { return transform.rotation; }
	Vector3 getSize() { return transform.scale; }

	virtual void update(float deltaTime);
	virtual void render();


	/// Add a child to the GameObject
	/// Add Component to the GameObject
	/// Add a Rigidbody to the GameObject if enabled

};

#endif
