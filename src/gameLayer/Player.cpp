#include "Player.h"

#include <raymath.h>

#include <SceneManager.h>

void Player::render2D()
{
	DrawRectangle(position2D.x, position2D.y, 32, 32, BLUE);

}

void Player::render3D()
{
	DrawSphere(rigidBody3D.translation, rigidBody3D.scale.x, BLUE);

	/// Show Directions
	DrawSphere(rigidBody3D.front + rigidBody3D.translation, 0.1f, RED);
	DrawSphere(rigidBody3D.back + rigidBody3D.translation, 0.1f, ORANGE);
	DrawSphere(rigidBody3D.left + rigidBody3D.translation, 0.1f, YELLOW);
	DrawSphere(rigidBody3D.right + rigidBody3D.translation, 0.1f, GREEN);
	DrawSphere(rigidBody3D.up + rigidBody3D.translation, 0.1f, BLUE);
	DrawSphere(rigidBody3D.down + rigidBody3D.translation, 0.1f, PURPLE);

}

void Player::update(float deltaTime)
{
	rigidBody3D.scale = Vector3(1, 1, 1);

	if (IsKeyDown(KEY_W)) rigidBody3D.translation += Vector3(rigidBody3D.front.x + (speed * deltaTime), 0, rigidBody3D.front.z + (speed * deltaTime));
	if (IsKeyDown(KEY_S)) rigidBody3D.translation += Vector3(rigidBody3D.back.x + (speed * deltaTime), 0, rigidBody3D.back.z + (speed * deltaTime));
	if (IsKeyDown(KEY_A)) rigidBody3D.translation += Vector3(rigidBody3D.left.x + (speed * deltaTime), 0, rigidBody3D.left.z + (speed * deltaTime));
	if (IsKeyDown(KEY_D)) rigidBody3D.translation += Vector3(rigidBody3D.right.x + (speed * deltaTime), 0, rigidBody3D.right.z + (speed * deltaTime));

	if (IsKeyPressed(KEY_SPACE)) rigidBody3D.jump(20);

	rigidBody3D.update(deltaTime);


	// Clamp 2D to position from game map size to screen size
	{
		const float screenX = static_cast<float>(GetScreenWidth());
		const float screenY = static_cast<float>(GetScreenHeight());

		position2D.x = screenX / 2 + rigidBody3D.translation.x;
		position2D.y = screenY / 2 + rigidBody3D.translation.z;

		//position3D.x = Clamp(position3D.x, -(screenX / 2), screenX / 2);
		//position3D.z = Clamp(position3D.z, -(screenY / 2), screenY / 2);

		position2D.x = Clamp(position2D.x, 0, screenX);
		position2D.y = Clamp(position2D.y, 0, screenY);

	}

}
