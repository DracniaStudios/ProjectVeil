#include "Physics.h"
#include <gameMap.h>

inline bool CheckCollisionAABB(const RigidBody3D& a, const RigidBody3D& b)
{
	const Cube aabbA = a.getAABB();
	const Cube aabbB = b.getAABB();

	return (aabbA.position.x < aabbB.position.x + aabbB.size.x &&
		aabbA.position.x + aabbA.size.x > aabbB.position.x &&
		aabbA.position.y < aabbB.position.y + aabbB.size.y &&
		aabbA.position.y + aabbA.size.y > aabbB.position.y &&
		aabbA.position.z < aabbB.position.z + aabbB.size.z &&
		aabbA.position.z + aabbA.size.z > aabbB.position.z);
}

void RigidBody3D::resolveConstrains()
{
	
}

void RigidBody3D::checkCollisionOnce(Vector3 position)
{
	
}

Vector3 RigidBody3D::performCollisionOnAxis(Vector3 position, Vector3 axis)
{
	return Vector3{ 0, 0, 0 };
}

/*
void RigidBody2D::resolveConstrains(GameMap& mapData)
{
	upTouch = downTouch = leftTouch = rightTouch = false;

	Vector2& pos = { translation.x, translation.y };

	float distance = Vector2Distance(lastPosition, pos);

	if (distance <= 0.01f)
	{
		return;
	}


	float GRANULARITY = 0.8f; // Adjust this value to increase/decrease the number of collision checks

	if (distance <= GRANULARITY)
	{
		checkCollisionOnce(pos, mapData);
	}
	else
	{
		Vector2 newPos = lastPosition;
		Vector2 delta = pos - lastPosition;
		delta = Vector2Normalize(delta);
		delta *= GRANULARITY * 0.99f;

		do
		{
			newPos += delta;
			Vector2 posText = newPos;
			checkCollisionOnce(newPos, mapData);

			// Check if checkCollisionOnce is changed
			if (newPos != posText)
			{
				pos = newPos;
				goto end;
			}
		} while (Vector2Length((newPos + delta) - pos) > GRANULARITY);

		checkCollisionOnce(pos, mapData);
	}
end:

	// End World Limits
	if (pos.x - scale.x / 2.f < 0) { pos.x = scale.x / 2.f; }
	if (pos.x + scale.x / 2.f > mapData.w) { pos.x = mapData.w - scale.x / 2.f; }
	if (pos.y + scale.y / 2.f > mapData.h) { pos.y = mapData.h - scale.y / 2.f; }

	if (leftTouch && velocity.x < 0) { velocity.x = 0; }
	if (rightTouch && velocity.x > 0) { velocity.x = 0; }

	if (upTouch && velocity.y < 0) { velocity.y = 0; }
	if (downTouch && velocity.y > 0) { velocity.y = 0; }

}

void RigidBody2D::checkCollisionOnce(Vector2& position, GameMap& mapData)
{
	Vector2 delta = position - lastPosition;

	Vector2 newPos = performCollisionOnOneAxis(mapData, { position.x, lastPosition.y }, Vector2{ delta.x, 0 });

	position = performCollisionOnOneAxis(mapData, { newPos.x, position.y }, Vector2{ 0, delta.y });
}

Vector2 RigidBody2D::performCollisionOnOneAxis(GameMap& mapData, Vector2 position, Vector2 delta)
{
	if (delta.x == 0 && delta.y == 0)
	{
		return position;
	}

	Vector2 dimensions = { scale.x, scale.y };

	int minX = floor(position.x - dimensions.x / 2.f - 1);
	int maxX = ceil(position.x + dimensions.x / 2.f + 1);

	int minY = floor(position.y - dimensions.y / 2.f - 1);
	int maxY = ceil(position.y + dimensions.y / 2.f + 1);

	minX = std::max(0, minX);
	minY = std::max(0, minY);
	maxX = std::min(mapData.w, maxX);
	maxY = std::min(mapData.h, maxY);

	for (int y = minY; y < maxY; y++)
	{
		for (int x = minX; x < maxX; x++)
		{
			if (mapData.getBlockUnsave(x, y).isCollidable())
			{
				Transform2D entity;
				entity.position = position;
				entity.w = dimensions.x;
				entity.h = dimensions.y;

				Transform2D block;
				block.position = Vector2{ x + 0.5f, y + 0.5f };
				block.w = 1;
				block.h = 1;

				if (entity.intersectTransform(block, -0.00005f))
				{
					if (delta.x != 0)
					{
						if (delta.x < 0) // Moving Left
						{
							leftTouch = true;
							position.x = x + 1.f + dimensions.x / 2.f;
							return position;
						}
						else
						{
							rightTouch = true;
							position.x = x - dimensions.x / 2.f;
							return position;
						}
					}
					else if (delta.y != 0)\
					{
						if (delta.y < 0) // Moving Up
						{
							upTouch = true;
							position.y = y + 1.f + dimensions.y / 2.f;
							return position;
						}
						else
						{
							downTouch = true;
							position.y = y - dimensions.y / 2.f;
							return position;
						}
					}
				}
			}
		}
	}
	return position;
}
*/