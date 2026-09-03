#pragma once
#ifndef COLLIDER_H
#define COLLIDER_H

#include <raylib.h>

class Collider
{
	Vector3 scale = Vector3{ 1.0f, 1.0f, 1.0f };


	bool isTrigger = false;

	void Start();

	void Update();

	void OnCollisionEnter(Collider * other);

	void OnCollisionStay(Collider * other);

	void OnCollisionExit(Collider * other);

	void OnTriggerEnter(Collider * other);

	void OnTriggerStay(Collider * other);

	void OnTriggerExit(Collider * other);

};

#endif	