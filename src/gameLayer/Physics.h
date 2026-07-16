#pragma once
#ifndef PHYSICS_H
#define PHYSICS_H

#include <raylib.h>
#include <raymath.h>
#include <nlohmann/json.hpp>

using Json = nlohmann::json;

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
	a.x /= scalar;
	a.y /= scalar;
	a.z /= scalar;
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
struct GameObject;

struct Transform3D : public Transform
{
	// translation
	// rotation
	// Scale

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
	
	Vector3 forward = Vector3(0, 0, 1); 
	Vector3 back = Vector3(0, 0, -1); 
	Vector3 right = Vector3(-1, 0, 0); 
	Vector3 left = Vector3(1, 0, 0); 
	Vector3 up = Vector3(0, 1, 0); 
	Vector3 down = Vector3(0, -1, 0); 
	
};


struct RigidBody3D : Transform3D
{
private:
	// pointer to owning GameObject
	GameObject* collidingWith = nullptr;

	Vector3 lastPosition = {};
	Vector3 appliedGravity = Vector3{0, 20, 0};

	float maxSpeed = 200.0f;
	float drag = 0.9f;

	bool useGravity = true;
	bool isColliding = false;

	float airTime = 0;
public:

	bool isEnabled = true;
	bool isStatic = false;
	bool canCollide = true;

	BoundingBox collisionBox = {};
	Vector3 angularVelocity = {}; // radians/sec, axis = spin axis
	Vector3 velocity = {};
	Vector3 acceleration = {};
	
	bool upTouch = false;
	bool downTouch = false; // isGrounded
	bool frontTouch = false; // X+1
	bool backTouch = false; // X-1
	bool leftTouch = false; // Z-1
	bool rightTouch = false; // Z+1

	// Set Values
	void SetGravity(Vector3 gravity) { appliedGravity = gravity; }
	void SetGravity(float x, float y, float z) { SetGravity(Vector3(x, y, z)); }
	void SetVelocity(Vector3 velocity) { this->velocity = velocity; }
	void SetVelocity(float x, float y, float z) { SetVelocity(Vector3(x, y, z)); }

	/// Get Values
	GameObject* GetCollider() const { return collidingWith; }
	BoundingBox* GetColliderBox() { return &collisionBox; }
	Vector3 GetVelocity() const { return velocity; }
	Vector3 GetAcceleration() const { return acceleration; }

	/// Force Application
	void ApplyGravity();
	void Teleport(Vector3 newPosition);
	void Teleport(float x, float y, float z);
	void AddForce(Vector3 forceDirection, float force);
	void Jump(float force);
	void UpdateForce(float deltaTime);
	void Update(float deltaTime);

	/// Collision Detection
	bool isCollidingWith(const RigidBody3D& other) const;
	Vector3 getCollisionNormal(const RigidBody3D& other) const;
	float getPenetrationDepth(const RigidBody3D& other) const;
	void checkRayCollision(const RigidBody3D& other);

	/// Constraint Resolution
	void resolveConstrains(GameObject* self, GameObject* otherObject);
	void resolveCollision(RigidBody3D& other);

	// Save & Load Data
	Json formatToJson();
	bool loadFromJson(const Json& j);

};

struct Transform2D : Transform
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

struct RigidBody2D : Transform2D
{
	Vector2 lastPosition = {};
	Vector2 velocity = {};
	Vector2 acceleration = {};

	float maxSpeed = 200.0f;
	float drag = 0.9f;
	
	bool isEnabled = true;
	bool isStatic = false;
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
	void applyGravity(Vector2 gravity = {0, 20}){	acceleration += gravity;}
	void applyForce(Vector2 force = { 1, 0 }) { acceleration += force; }

	void updateForce(float deltaTime);

	void update(float deltaTime);

	Vector2 getPosition() { return Vector2{ translation.x, translation.y }; }

	void jump(float force)
	{
		velocity.y = 0;
		velocity.y -= force;
	}

	void resolveConstrains(GameMap& mapData);

	void checkCollisionOnce(Vector2& position, GameMap& mapData);

	Vector2 performCollisionOnOneAxis(GameMap& mapData, Vector2 position, Vector2 delta);
};


#endif
