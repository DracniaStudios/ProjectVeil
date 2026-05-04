#include "Player.h"

#include <raymath.h>
#include <SceneManager.h>

bool displayBody2D = true;
bool displayBody3D = false;
bool displayDirection = true;

void Player::render2D()
{
	if (!isEnabled) return;

	if (displayBody2D) {
		DrawRectangle(rigidBody2D.translation.x, rigidBody2D.translation.y, 32, 32, BLUE);
	}
}

void Player::render3D()
{
	if (!isEnabled) return;
	//DrawSphere(rigidBody3D.translation, rigidBody3D.scale.x, BLUE);

	if (displayBody3D) {
		DrawBoundingBox(rigidBody3D.collisionBox, WHITE);
		DrawModel(model, rigidBody3D.translation, 1.0f, Color{ 20, 30, 30, 255 });
		DrawModelWires(model, rigidBody3D.translation, 1.0f, BLACK);
	}
	if (displayDirection) {
		/// Show Directions
		DrawSphere(rigidBody3D.front + rigidBody3D.translation, 0.1f, RED);
		DrawSphere(rigidBody3D.back + rigidBody3D.translation, 0.1f, ORANGE);
		DrawSphere(rigidBody3D.left + rigidBody3D.translation, 0.1f, YELLOW);
		DrawSphere(rigidBody3D.right + rigidBody3D.translation, 0.1f, GREEN);
		DrawSphere(rigidBody3D.up + rigidBody3D.translation, 0.1f, BLUE);
		DrawSphere(rigidBody3D.down + rigidBody3D.translation, 0.1f, PURPLE);
	}

}

void Player::update(Camera* camera, float deltaTime)
{
	if (!isEnabled) return;
	/// Player Flags
	isCrouching = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT);

	/// Player Scale
	rigidBody3D.scale = Vector3(1, 2, 1);

	/// Player Movement
	auto speed = baseSpeed * deltaTime;


	if (IsKeyDown(KEY_W)) { 
		rigidBody3D.translation += (rigidBody3D.front * 0.1f);// * speed;
	}
	if (IsKeyDown(KEY_S)) { 
		rigidBody3D.translation += (rigidBody3D.back * 0.1f);// * speed;
	}
	if (IsKeyDown(KEY_A)) { 
		rigidBody3D.translation += (rigidBody3D.left * 0.1f);// * speed;
	}
	if (IsKeyDown(KEY_D)) {
		rigidBody3D.translation += (rigidBody3D.right * 0.1f);// * speed;
	}

	if (IsKeyPressed(KEY_SPACE)) rigidBody3D.jump(20);

	rigidBody3D.update(deltaTime);


	// Clamp 2D to position from game map size to screen size
	{
		const float screenX = static_cast<float>(GetScreenWidth());
		const float screenY = static_cast<float>(GetScreenHeight());

		rigidBody2D.translation.x = screenX / 2 + rigidBody3D.translation.x;
		rigidBody2D.translation.y = screenY / 2 + rigidBody3D.translation.z;

		//position3D.x = Clamp(position3D.x, -(screenX / 2), screenX / 2);
		//position3D.z = Clamp(position3D.z, -(screenY / 2), screenY / 2);

		rigidBody2D.translation.x = Clamp(rigidBody2D.translation.x, 0, screenX);
		rigidBody2D.translation.y = Clamp(rigidBody2D.translation.y, 0, screenY);

	}

}