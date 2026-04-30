#include "Player.h"

#include <raymath.h>

void Player::render2D()
{
	DrawRectangle(position2D.x, position2D.y, 32, 32, BLUE);

}

void Player::render3D()
{
	DrawSphere(rigidBody3D.translation, 0.5f, BLUE);
}

void Player::update(float deltaTime)
{


	if (IsKeyDown(KEY_W)) rigidBody3D.translation.z -= speed * deltaTime;
	if (IsKeyDown(KEY_S)) rigidBody3D.translation.z += speed * deltaTime;
	if (IsKeyDown(KEY_A)) rigidBody3D.translation.x -= speed * deltaTime;
	if (IsKeyDown(KEY_D)) rigidBody3D.translation.x += speed * deltaTime;

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
