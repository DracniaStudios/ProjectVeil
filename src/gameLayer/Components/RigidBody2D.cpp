#include "Physics.h"

#include <cmath>

namespace Physics2D {
	void RigidBody2D::updateForce(float deltaTime)
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

		// Ground Rules
		// Bounds rather than the window, so a body runs the same with no window open
		if (bounds.width > 0 && bounds.height > 0)
		{
			if (translation.y > bounds.y + bounds.height - scale.y) { translation.y = bounds.y + bounds.height - scale.y; }
			if (translation.y < bounds.y + scale.y) { translation.y = bounds.y + scale.y; }
			if (translation.x > bounds.x + bounds.width - scale.x) { translation.x = bounds.x + bounds.width - scale.x; }
			if (translation.x < bounds.x + scale.x) { translation.x = bounds.x + scale.x; }
		}


	}
	void RigidBody2D::update(float deltaTime)
	{
		if (scale == Vector3Zero()) { scale = Vector3One(); }

		if (!isEnabled) return;
		if (isStatic) return;

		if (useGravity) applyGravity();

		updateForce(deltaTime);

		lastPosition = Vector2(translation.x, translation.y);

	}

	// Both return a raylib Rectangle, whose last two fields are a size. They used
	// to be handed the far corner instead, so every collision test against them
	// was against a rectangle stretching from the body to twice its distance from
	// the origin — FlappyBird's "left the green area" check could all but never fire.
	Rectangle RigidBody2D::getRectCentered() {
		// Calculates As Center Position
		return Rectangle(translation.x - scale.x / 2,
			translation.y - scale.y / 2,
			scale.x,
			scale.y
		);
	}
	Rectangle RigidBody2D::getRectCornered() {
		// Calculates As Top Left Corner Position
		return Rectangle(translation.x,
			translation.y,
			scale.x,
			scale.y
		);
	}
}