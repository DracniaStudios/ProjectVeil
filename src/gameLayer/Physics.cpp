#include "Physics.h"
#include <gameMap.h>
#include <iostream>

#include <asserts.h>


void RigidBody3D::resolveConstrains(RigidBody3D* otherObjects, int objectCount)
{
	if (otherObjects == nullptr || objectCount <= 0)
		return;

	for (int i = 0; i < objectCount; i++)
	{
		if (&otherObjects[i] <= this) { continue; }
		checkCollision(otherObjects[i]);
	}
}

bool RigidBody3D::checkCollision(RigidBody3D& collider)
{
	if (CheckCollisionBoxes(collisionBox, collider.collisionBox)) {
		// Calculate overlap on each axis
		float overlapLeft = collisionBox.max.x - collider.collisionBox.min.x;    // This object's right vs other's left
		float overlapRight = collider.collisionBox.max.x - collisionBox.min.x;   // Other's right vs this object's left
		float overlapBottom = collisionBox.max.y - collider.collisionBox.min.y;  // This object's top vs other's bottom
		float overlapTop = collider.collisionBox.max.y - collisionBox.min.y;     // Other's top vs this object's bottom
		float overlapBack = collisionBox.max.z - collider.collisionBox.min.z;    // This object's front vs other's back
		float overlapFront = collider.collisionBox.max.z - collisionBox.min.z;   // Other's front vs this object's back

		// Find the minimum overlap (collision direction)
		float minOverlapX = std::min(overlapLeft, overlapRight);
		float minOverlapY = std::min(overlapBottom, overlapTop);
		float minOverlapZ = std::min(overlapBack, overlapFront);

		if (minOverlapX <= minOverlapY && minOverlapX <= minOverlapZ) {
			// Collision on X axis - resolve both objects
			float pushDistance = minOverlapX * 0.5f; // Split the correction between both objects

			if (overlapLeft <= overlapRight) {
				// This object is to the right, push it right
				translation.x += pushDistance;
				collider.translation.x -= pushDistance;
				rightTouch = true;
				collider.leftTouch = true;
				std::cout << "Collided on X axis! Right\n";
			}
			else {
				// This object is to the left, push it left
				translation.x -= pushDistance;
				collider.translation.x += pushDistance;
				leftTouch = true;
				collider.rightTouch = true;
				std::cout << "Collided on X axis! Left\n";
			}
			velocity.x = 0;
			collider.velocity.x = 0;
		}
		else if (minOverlapY <= minOverlapX && minOverlapY <= minOverlapZ) {
			// Collision on Y axis - resolve both objects
			float pushDistance = minOverlapY * 0.5f;

			if (overlapBottom <= overlapTop) {
				// This object is above, push it up
				translation.y += pushDistance;
				collider.translation.y -= pushDistance;
				upTouch = true;
				collider.downTouch = true;
				std::cout << "Collided on Y axis! Up\n";
			}
			else {
				// This object is below, push it down
				translation.y -= pushDistance;
				collider.translation.y += pushDistance;
				downTouch = true;
				collider.upTouch = true;
				std::cout << "Collided on Y axis! Down\n";
				collider.acceleration += Vector3{ 0, 50, 0 };
			}
			velocity.y = 0;
			collider.velocity.y = 0;
		}
		else {
			// Collision on Z axis - resolve both objects
			float pushDistance = minOverlapZ * 0.5f;

			if (overlapBack <= overlapFront) {
				// This object is in front, push it forward
				translation.z += pushDistance;
				collider.translation.z -= pushDistance;
				frontTouch = true;
				collider.backTouch = true;
				std::cout << "Collided on Z axis! Front\n";
			}
			else {
				// This object is behind, push it back
				translation.z -= pushDistance;
				collider.translation.z += pushDistance;
				backTouch = true;
				collider.frontTouch = true;
				std::cout << "Collided on Z axis! Back\n";
			}
			velocity.z = 0;
			collider.velocity.z = 0;
		}
		return true;
	}
	return false;
}