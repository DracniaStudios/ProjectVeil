#pragma once
#ifndef PHYSICS_H
#define PHYSICS_H

#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include <string>

#if defined(RAYMATH_DISABLE_CPP_OPERATORS)
// Vector2 operator overloads (only defined when raymath C++ operators are disabled)

/// Add
inline Vector2 operator+(const Vector2& a, const Vector2& b)
{
	return { a.x + b.x, a.y + b.y };
}
inline Vector3 operator+(const Vector3& a, const Vector3& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

/// Subtract
inline Vector2 operator-(const Vector2& a, const Vector2& b)
{
	return { a.x - b.x, a.y - b.y };
}
inline Vector3 operator-(const Vector3& a, const Vector3& b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

/// Multiply
inline Vector2 operator*(const Vector2& a, float scalar)
{
	return { a.x * scalar, a.y * scalar };
}

inline Vector3 operator*(const Vector3& a, float scalar)
{
	return { a.x * scalar, a.y * scalar, a.z * scalar };
}

/// Divide
inline Vector2 operator/(const Vector2& a, float scalar)
{
	return { a.x / scalar, a.y / scalar };
}


inline Vector3 operator/(const Vector3& a, float scalar)
{
	return { a.x / scalar, a.y / scalar, a.z / scalar };
}

// Multiply by scalar

inline Vector2& operator*=(Vector2& a, float scalar)
{
	a.x *= scalar;
	a.y *= scalar;
	return a;
}
inline Vector3& operator*=(Vector3& a, float scalar)
{
	a.x *= scalar;
	a.y *= scalar;
	a.z *= scalar;
	return a;
}

/// Divide By Scalar
inline Vector2& operator/=(Vector2& a, float scalar)
{
	a.x /= scalar;
	a.y /= scalar;
	return a;
}

inline Vector3& operator/=(Vector3& a, float scalar)
{
	a.x *= scalar;
	a.y *= scalar;
	a.z *= scalar;
	return a;
}

/// Add By Scalar
inline Vector2& operator+=(Vector2& a, float scalar)
{
	a.x += scalar;
	a.y += scalar;
	return a;
}
inline Vector3& operator+=(Vector3& a, float scalar)
{
	a.x += scalar;
	a.y += scalar;
	a.z += scalar;
	return a;
}

/// Subtract By Scalar

inline Vector2& operator-=(Vector2& a, float scalar)
{
	a.x -= scalar;
	a.y -= scalar;
	return a;
}
inline Vector3& operator-=(Vector3& a, float scalar)
{
	a.x -= scalar;
	a.y -= scalar;
	a.z -= scalar;
	return a;
}

/// Comparison
inline bool operator==(const Vector2& a, const Vector2& b)
{
	return a.x == b.x && a.y == b.y;
}
inline bool operator==(const Vector3& a, const Vector3& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

// Not 
inline bool operator!=(const Vector2& a, const Vector2& b)
{
	return !(a == b);
}
inline bool operator!=(const Vector3& a, const Vector3& b)
{
	return !(a == b);
}

inline Vector2& operator+=(Vector2& a, const Vector2& b)
{
	a.x += b.x;
	a.y += b.y;
	return a;
}
inline Vector3& operator+=(Vector3& a, const Vector3& b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	return a;
}

inline Vector2& operator-=(Vector2& a, const Vector2& b)
{
	a.x -= b.x;
	a.y -= b.y;
	return a;
}
inline Vector3& operator-=(Vector3& a, const Vector3& b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	return a;
}

inline Vector2& operator*=(Vector2& a, const Vector2& b)
{
	a.x *= b.x;
	a.y *= b.y;
	return a;
}
inline Vector3& operator*=(Vector3& a, const Vector3& b)
{
	a.x *= b.x;
	a.y *= b.y;
	a.z *= b.z;
	return a;
}

inline Vector2& operator/=(Vector2& a, const Vector2& b)
{
	a.x /= b.x;
	a.y /= b.y;
	return a;
}

inline Vector3& operator/=(Vector3& a, const Vector3& b)
{
	a.x /= b.x;
	a.y /= b.y;
	a.z /= b.z;
	return a;
}
#endif // RAYMATH_DISABLE_CPP_OPERATORS

struct GameMap;

struct Transform3D : public Transform
{
	// translation
	// rotation
	// Scale

	BoundingBox collisionBox = {};

	Vector3& getPosition() { return translation; }
	Vector3 getCenter()		  const { return { translation.x, translation.y, translation.z }; }

	Vector3 getTop()          const { return { translation.x, translation.y - scale.y * 0.5f, translation.z }; }
	Vector3 getBottom()       const { return { translation.x, translation.y + scale.y * 0.5f, translation.z }; }
	Vector3 getLeft()         const { return { translation.x - scale.x * 0.5f, translation.y, translation.z }; }
	Vector3 getRight()        const { return { translation.x + scale.x * 0.5f, translation.y, translation.z }; }
	Vector3 getTopLeft()      const { return { translation.x - scale.x * 0.5f, translation.y - scale.y * 0.5f, translation.z }; }
	Vector3 getTopRight()     const { return { translation.x + scale.x * 0.5f, translation.y - scale.y * 0.5f, translation.z }; }
	Vector3 getBottomLeft()   const { return { translation.x - scale.x * 0.5f, translation.y + scale.y * 0.5f, translation.z }; }
	Vector3 getBottomRight()  const { return { translation.x + scale.x * 0.5f, translation.y + scale.y * 0.5f, translation.z }; }

	Rectangle getAABB() const { return { translation.x - scale.x * 0.5f, translation.y - scale.y * 0.5f, scale.x, scale.y }; }
	
	// Z+ is Forward
	// X+ is Right
	
	Vector3 front = Vector3(0, 0, 1); 
	Vector3 back = Vector3(0, 0, -1); 
	Vector3 right = Vector3(-1, 0, 0); 
	Vector3 left = Vector3(1, 0, 0); 
	Vector3 up = Vector3(0, 1, 0); 
	Vector3 down = Vector3(0, -1, 0); 
	
};


struct RigidBody3D : public Transform3D
{

	Vector3 lastPosition = {};
	Vector3 velocity = {};
	Vector3 acceleration = {};
	BoundingBox collisionBox = {};

	float maxSpeed = 200.0f;
	float drag = 0.9f;
	
	bool useGravity = true;
	bool isStatic = false;

	bool upTouch = false;
	bool downTouch = false; // isGrounded
	bool frontTouch = false; // X+1
	bool backTouch = false; // X-1
	bool leftTouch = false; // Z-1
	bool rightTouch = false; // Z+1

	void teleport(Vector3 newPosition)
	{
		translation = newPosition;
		lastPosition = newPosition;
	}

	float airTime = 0;
	void applyGravity()
	{
		acceleration -= Vector3{ 0, 20, 0 }; //Vector3{ 0, 9.81f, 0 };
	}

	void updateForce(float deltaTime)
	{
		if (airTime > 0)
		{
			velocity.y += deltaTime;
		}

		// Apply acceleration to velocity
		velocity += acceleration * deltaTime;
		// Limit velocity to max speed
		if (Vector3Length(velocity) > maxSpeed)
		{
			velocity = Vector3Scale(Vector3Normalize(velocity), maxSpeed);
		}
		// Apply velocity to position
		translation += velocity * deltaTime;
		// Apply drag to velocity
		velocity *= drag;
		// Reset acceleration for the next frame
		acceleration = { 0, 0 };

		if (translation.y < 0.0f + scale.y / 2)
		{
			translation.y = 0.0f + scale.y / 2;
			velocity.y = 0.0f;
			downTouch = true;
		}
		else
		{
			downTouch = false;
			airTime += deltaTime;
		}

	}

	void update(float deltaTime)
	{
		if (!isStatic) {
			if (useGravity) applyGravity();
			updateForce(deltaTime);
		}
		lastPosition = translation;

		// Update collision box to match current position and scale
		collisionBox.min = { translation.x - scale.x / 2, translation.y - scale.y / 2, translation.z - scale.z / 2 };
		collisionBox.max = { translation.x + scale.x / 2, translation.y + scale.y / 2, translation.z + scale.z / 2 };
	}

	void jump(float force){	if (downTouch) { velocity.y = force; }}

	void addForce(Vector3 forceDirection, float force)
	{
		acceleration += Vector3Scale(forceDirection, force);
	}


	// Collision Detection
	/// Resolve constraints with other dynamic and static objects
	/// Pass an array of other RigidBody3D objects to check collisions against
	void resolveConstrains(RigidBody3D* otherObjects = nullptr, int objectCount = 0);

	bool checkCollision(RigidBody3D& collider);
};

struct Transform2D : public Transform
{
	// translation
	// rotation
	// scale

	Vector2 getCenter()		  const { return { translation.x, translation.y };  }
	Vector2 getTop()          const { return { translation.x, translation.y - scale.y * 0.5f }; }
	Vector2 getBottom()       const { return { translation.x, translation.y + scale.y * 0.5f }; }
	Vector2 getLeft()         const { return { translation.x - scale.x * 0.5f, translation.y }; }
	Vector2 getRight()        const { return { translation.x + scale.x * 0.5f, translation.y }; }
	Vector2 getTopLeft()      const { return { translation.x - scale.x * 0.5f, translation.y - scale.y * 0.5f }; }
	Vector2 getTopRight()     const { return { translation.x + scale.x * 0.5f, translation.y - scale.y * 0.5f }; }
	Vector2 getBottomLeft()   const { return { translation.x - scale.x * 0.5f, translation.y + scale.y * 0.5f }; }
	Vector2 getBottomRight()  const { return { translation.x + scale.x * 0.5f, translation.y + scale.y * 0.5f }; }

	Rectangle getAABB() const { return { translation.x - scale.x * 0.5f, translation.y - scale.y * 0.5f, scale.x, scale.y }; }

	bool intersectPoint(Vector2 point, float delta = 0) const
	{
		Rectangle aabb = getAABB();
		aabb.x -= delta;
		aabb.y -= delta;
		aabb.width += delta * 2;
		aabb.height += delta * 2;

		return CheckCollisionPointRec(point, aabb);
	}

	bool intersectTransform(Transform2D other, float delta = 0) const
	{
		Rectangle a = getAABB();
		Rectangle b = other.getAABB();

		a.x -= delta;
		a.y -= delta;
		a.width += delta * 2;
		a.height += delta * 2;

		b.x -= delta;
		b.y -= delta;
		b.width += delta * 2;
		b.height += delta * 2;

		return CheckCollisionRecs(a, b);
	}
};

struct RigidBody2D : public Transform2D
{
	Vector2 lastPosition = {};
	Vector2 velocity;
	Vector2 acceleration;

	float maxSpeed = 200.0f;
	float drag = 0.9f;
	
	bool useGravity = true;

	bool upTouch = false;
	bool downTouch = false;
	bool leftTouch = false; // Z-1
	bool rightTouch = false; // Z+1

	void teleport(Vector2 newPosition)
	{
		translation = Vector3(newPosition.x, newPosition.y, 0);
		lastPosition = newPosition;
	}

	// Force Functions
	void applyGravity()
	{
		acceleration += {0, 20};
	}

	void updateForce(float deltaTime)
	{
		velocity += acceleration * deltaTime;
		if (Vector2Length(velocity) > maxSpeed)
		{
			velocity = Vector2Scale(Vector2Normalize(velocity), maxSpeed);
		}
		translation.x += velocity.x * deltaTime;
		translation.y += velocity.y * deltaTime;

		// Universal drag ( air resisitance, friction, etc. )
		Vector2 dragVector = Vector2{ velocity.x * std::abs(velocity.x), velocity.y * std::abs(velocity.y) };
		float drag = 0.01f; // Adjust this value to increase/decrease drag strength

		if (Vector2Length(dragVector) * drag * deltaTime > Vector2Length(velocity))
		{
			velocity = {};
		}
		else
		{
			velocity -= dragVector * drag * deltaTime;
		}

		if (Vector2Length(velocity) <= 0.01f)
		{
			velocity = {};
		}

		acceleration = {};
	}

	void update(float deltaTime)
	{
		if (useGravity) applyGravity();
		updateForce(deltaTime);
		lastPosition = Vector2(translation.x, translation.y);
	}

	void jump(float force)	{ if (downTouch){velocity.y -= force;}	}

	void resolveConstrains(GameMap& mapData);

	void checkCollisionOnce(Vector2& position, GameMap& mapData);

	Vector2 performCollisionOnOneAxis(GameMap& mapData, Vector2 position, Vector2 delta);
};


#endif
