#include "Physics.h"

#include <gameMap.h>
#include <iostream>
#include <SceneManager.h>

void RigidBody2D::resolveConstrains(GameMap& mapData)
{

}

void RigidBody2D::checkCollisionOnce(Vector2& position, GameMap& mapData)
{

}

Vector2 RigidBody2D::performCollisionOnOneAxis(GameMap& mapData, Vector2 position, Vector2 delta)
{
	return Vector2Zero();
}

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

	if (translation.y > GetScreenHeight() - scale.y) { translation.y = GetScreenHeight() - scale.y; }
	if (translation.y < scale.y) { translation.y = scale.y; }
	if (translation.x > GetScreenWidth() - scale.x) { translation.x = GetScreenWidth() - scale.x; }
	if (translation.x < scale.x) { translation.x = scale.x; }


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

Rectangle RigidBody2D::getRectCentered() {
	// Calculates As Center Position
	auto halfScale = Vector2(scale.x / 2, scale.y / 2);
	return Rectangle(translation.x - halfScale.x,
		translation.y - halfScale.y,
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