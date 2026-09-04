#pragma once
#ifndef PHYSICS_H
#define PHYSICS_H

#include <algorithm>
#include <cstdint>
#include <vector>

#include <raylib.h>
#include <raymath.h>
#include <nlohmann/json.hpp>

#include <Collider.h>

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
	// GameObjects that had multiple contacts on a single frame no longer linger together
	// This could cause the OnCollision functions to run multiple times.
	std::vector<GameObject*> contactsThisFrame;
	std::vector<GameObject*> contactsLastFrame;

	// GameObjects that are looked up by ID.
	std::vector<std::uint64_t> triggerContactsThisFrame;
	std::vector<std::uint64_t> triggerContactsLastFrame;

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

	/**
	 * Axis-aligned box enclosing the collider — the BROAD phase, and nothing else.
	 *
	 * Deliberately not called `broadPhaseBox`: it is not the collision shape, it is
	 * a conservative bound around it. Because it can only ever be larger than the
	 * collider, a miss here is a guaranteed miss, which is exactly what makes it a
	 * sound gate in front of the exact test — and exactly why nothing should use
	 * it to answer a question. Anything asking "is something solid here" wants
	 * Raycast() or getContact() instead.
	 */
	BoundingBox broadPhaseBox = {};

	/**
	 * The collision volume, authored independently of the render scale.
	 *
	 * This replaces `broadPhaseBoxScale`, which was declared for exactly this and
	 * never read by anything. Its defaults — size {1,1,1}, offset {0,0,0},
	 * COLLIDER_BOX, COLLIDER_COLLISION — make every accessor below return what it
	 * returned when the collision shape was simply `scale`, so an object that has
	 * never had its collider touched behaves exactly as it always did.
	 */
	Collider3D collider = {};
	Vector3 angularVelocity = {}; // radians/sec, axis = spin axis
	Vector3 velocity = {};
	Vector3 acceleration = {};
	bool lockAngularVelocity = false;
	bool lockVelocity = false;
	bool lockAcceleration = false;
	bool lockTranslation = false;
	bool lockRotation = false;
	bool lockScale = false;
	
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
	GameObject* GetCollider() const { return contactsThisFrame.empty() ? nullptr : contactsThisFrame.front(); }
	Vector3 GetVelocity() const { return velocity; }
	Vector3 GetAcceleration() const { return acceleration; }
	float GetAirTime() const { return airTime; }

	/**
	 * World-space half-extents of the collision volume, accounting for rotation.
	 *
	 * The solver is axis-aligned, so a rotated body is represented by the
	 * smallest axis-aligned box that still contains it. Every half-extent in the
	 * collision path goes through here, so the box, the penetration depth, the
	 * contact normal and the touch rays can never disagree about how big a body
	 * is.
	 *
	 * Returns exactly `scale * 0.5f` for an unrotated body, so nothing changes
	 * for the axis-aligned common case.
	 *
	 * This is the BROAD-phase volume. The narrow phase (getContact) uses the
	 * true oriented box and does not inherit this slack.
	 */
	Vector3 GetWorldHalfExtents() const { return ColliderWorldHalfExtents(GetColliderVolume()); }

	/**
	 * The collider resolved into world space — shape, centre, axes and extents.
	 *
	 * This is what the narrow phase actually tests, and every geometry query on
	 * this body is a thin read off it, so the solver, the broad-phase box, the
	 * touch rays and the debug draw cannot disagree about where a body is or how
	 * big it is.
	 */
	//
	// Defined here rather than in RigidBody3D.cpp on purpose. These are one-line
	// compositions of the collider with the transform and need nothing from the
	// engine, whereas RigidBody3D.cpp reaches SceneManager inside UpdateForce and
	// so cannot be linked on its own. Keeping them inline lets a translation unit
	// that only asks geometry questions — SoundField's occlusion pass, say — link
	// against Collider3D.cpp alone.
	ColliderVolume GetColliderVolume() const
	{
		return MakeColliderVolume(collider, translation, rotation, scale);
	}

	/**
	 * World-space centre of the collision volume.
	 *
	 * Deliberately not the same thing as getCenter(), which is the transform's
	 * own position: a collider may be offset from the body it rides on, and the
	 * collision path wants the volume while the render and audio paths want the
	 * object.
	 */
	Vector3 GetColliderCenter() const { return GetColliderVolume().center; }

	/** Half-extents along the collider's own axes. `scale * 0.5f` by default. */
	Vector3 GetLocalHalfExtents() const { return collider.GetLocalHalfExtents(scale); }

	/// Editor Support
	// Rebuilds the AABB from the current translation/scale without stepping the
	// simulation, and repairs a degenerate scale/rotation on the way through.
	//
	// Update() normally does both, but the World Editor freezes the simulation
	// while objects are being manipulated — and mouse picking reads broadPhaseBox.
	// Without this the editor would only be able to re-click an object where it
	// used to be, and a zeroed quaternion would never be repaired.
	void SyncBroadPhaseBox();

	/// Force Application
	void ApplyGravity();
	void Teleport(Vector3 newPosition);
	void Teleport(float x, float y, float z);
	void AddForce(Vector3 forceDirection, float force);
	void Jump(float force);
	void UpdateForce(float deltaTime);
	void Update(float deltaTime);

	/**
	 * Fires onTriggerExit for anything that left this body's trigger volumes, then
	 * rolls the trigger contact lists ready for this frame's solver.
	 *
	 * Called once per body per frame from the two places that tick a body —
	 * GameObject::update and Player::update3D — rather than from Update(), because
	 * it needs the owning object and the map to resolve IDs, and because a
	 * disabled body must still report the contacts it just lost.
	 */
	void DispatchTriggerEvents(GameObject* self, GameMap* map);

	/// Collision Detection
	/**
	 * Narrow phase: exact oriented-box overlap by the separating axis theorem.
	 *
	 * Two convex shapes are apart if and only if some axis exists on which their
	 * projections do not overlap. For two boxes it is enough to test 15
	 * candidates — each box's 3 face normals, plus the 9 cross products of their
	 * edge directions. A gap on any one proves separation and returns early; if
	 * all 15 overlap, the smallest overlap is the contact normal and depth.
	 *
	 * The 9 cross products are not optional: without them two boxes can pass
	 * through one another corner-first, which is exactly the case an
	 * axis-aligned test cannot see.
	 */
	ContactInfo getContact(const RigidBody3D& other) const;

	/**
	 * Ray against this body's collider — exact, unlike testing broadPhaseBox.
	 *
	 * Anything asking "is there something solid here" wants this rather than the
	 * broad-phase box: that box is a conservative enclosure, so a rotated ramp, an
	 * offset collider or a sphere all report hits through the empty space between
	 * the two. Returns the exit distance when the ray starts inside; see
	 * ColliderRaycast for the full contract.
	 */
	bool Raycast(Ray ray, float& outDistance, Vector3& outNormal) const
	{
		return ColliderRaycast(GetColliderVolume(), ray, outDistance, outNormal);
	}

	/**
	 * Cheap conservative overlap test between two bodies' broad-phase boxes.
	 *
	 * The gate in front of getContact(): the box encloses the collider, so a miss
	 * here means the colliders cannot be touching and the 15-axis test can be
	 * skipped outright. A hit means only "worth testing properly".
	 */
	bool OverlapsBroadPhase(const RigidBody3D& other) const
	{
		return CheckCollisionBoxes(broadPhaseBox, other.broadPhaseBox);
	}

	// Convenience wrappers over getContact(). Prefer getContact() when more than
	// one of these is needed — each of these runs a full separating-axis test.
	bool isCollidingWith(const RigidBody3D& other) const;
	Vector3 getCollisionNormal(const RigidBody3D& other) const;
	float getPenetrationDepth(const RigidBody3D& other) const;
	void checkRayCollision(const RigidBody3D& other);

	/// Constraint Resolution
	void resolveConstrains(GameObject* self, GameObject* otherObject);
	void resolveCollision(RigidBody3D& other);
	// Resolves against an already-computed contact, so the solver does not repeat
	// the narrow-phase test it just ran
	void resolveCollision(RigidBody3D& other, const ContactInfo& contact);

	// Save & Load Data
	Json formatToJson();
	bool loadFromJson(const Json& j);

};

/**
 * Wireframe box that honours rotation, unlike raylib's axis-aligned
 * DrawCubeWires/DrawBoundingBox. Used by the collider debug draw and by the
 * World Editor's selection outline so both match what render3D actually draws.
 */
void DrawOrientedBoxWires(Vector3 center, Vector3 size, Quaternion rotation, Color color);

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
	bool canMove = true;

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
	Rectangle getRectCentered();
	Rectangle getRectCornered();

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
