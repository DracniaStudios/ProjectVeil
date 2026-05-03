#include "Physics.h"
#include <gameMap.h>
#include <iostream>


void RigidBody3D::resolveConstrains(RigidBody3D* otherObjects, int objectCount)
{
	if (otherObjects == nullptr || objectCount <= 0)
		return;

	for (int i = 0; i < objectCount; i++)
	{
		if (&otherObjects[i] == this) { continue; }

		if (CheckCollisionBoxes(collisionBox, otherObjects[i].collisionBox))
		{
			resolveCollision(otherObjects[i]);
		}
	}
}

void checkCollision(std::string axis)
{
	if (axis == "x")
	{
		std::cout << "Collided on X axis!" << std::endl;
		if (for)
	}
	else if (axis == "y")
	{
		std::cout << "Collided on Y axis!" << std::endl;
	}
	else if (axis == "z")
	{
		std::cout << "Collided on Z axis!" << std::endl;
	}
}

void RigidBody3D::resolveCollision(RigidBody3D& other)
{
	// this bounding box
	BoundingBox otherBox = other.collisionBox;

	if (isStatic && other.isStatic) return; // No need to resolve collision between two static objects
	
	if (isStatic && !other.isStatic) {
		other.resolveCollision(*this); // Let the non-static object handle the collision response
		return;
	}

	if (!isStatic && other.isStatic) {
		// Calculate the overlap between the two objects
		Vector3 overlap = {
			std::min(translation.x + scale.x / 2, other.translation.x + other.scale.x / 2) - std::max(translation.x - scale.x / 2, other.translation.x - other.scale.x / 2),
			std::min(translation.y + scale.y / 2, other.translation.y + other.scale.y / 2) - std::max(translation.y - scale.y / 2, other.translation.y - other.scale.y / 2),
			std::min(translation.z + scale.z / 2, other.translation.z + other.scale.z / 2) - std::max(translation.z - scale.z / 2, other.translation.z - other.scale.z / 2)
		};
	}

}
