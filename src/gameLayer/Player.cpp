#include "Player.h"

#include <raymath.h>
#include <SceneManager.h>

void Player::onEnable()
{
	stamina = getMaxStamina();
	rigidBody2D.translation = Vector3(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f, 0);
	std::cout << rigidBody2D.translation.x << "\n";
	GameObject::onEnable();
}

void Player::onDisable()
{
	GameObject::onDisable();
	
}

void Player::render2D()
{
	if (!isEnabled) return;
	
	DrawCircle(rigidBody2D.translation.x, rigidBody2D.translation.y, 5, WHITE);
	DrawRectangle(rigidBody2D.translation.x, rigidBody2D.translation.y, rigidBody2D.scale.x, rigidBody2D.scale.y, WHITE);

}

void Player::render3D()
{
	if (!isEnabled) { return; }

	if (displayCollider)
	{
		DrawBoundingBox(rigidBody3D.collisionBox, WHITE);
	}

	if (display3DModel) {
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

void Player::update2D(SceneManager* manager, float deltaTime)
{
	if (!isEnabled) return;
	if (!manager->currentScene->is2DActive) return;

	// Player2D Function
	moveDirection = Vector2Zero();
	moveDirection.x = IsKeyDown(KEY_A) ? -1.0f : IsKeyDown(KEY_LEFT) ? -1.0f : IsKeyDown(KEY_D) ? 1.0f : IsKeyDown(KEY_RIGHT) ? 1.0f : 0;
	moveDirection.y = IsKeyDown(KEY_W) ? -1.0f : IsKeyDown(KEY_UP) ? -1.0f : IsKeyDown(KEY_S) ? 1.0f : IsKeyDown(KEY_DOWN) ? 1.0f : 0;
	
	auto speed = IsKeyDown(KEY_LEFT_SHIFT) ? baseSpeed * 2 : baseSpeed;
	
	rigidBody2D.translation += Vector3(moveDirection.x, moveDirection.y) * speed;
	//rigidBody2D.update(deltaTime);
}

void Player::update3D(SceneManager* manager, float deltaTime)
{
	if (!isEnabled) return;
	if (manager->currentScene->is2DActive) { return; }

	/// Player Flags
	isCrouching = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT);

	/// Player Scale
	rigidBody3D.scale = Vector3(1, 2, 1);

	/// Player Movement
	auto speed = IsKeyDown(KEY_LEFT_SHIFT) ? baseSpeed * 2 : baseSpeed;

	// Player Movement Input
	moveDirection = Vector2Zero();
	moveDirection.x = IsKeyDown(KEY_A) ? -1.0f : IsKeyDown(KEY_LEFT) ? -1.0f : IsKeyDown(KEY_D) ? 1.0f : IsKeyDown(KEY_RIGHT) ? 1.0f : 0;
	moveDirection.y = IsKeyDown(KEY_W) ? 1.0f : IsKeyDown(KEY_UP) ? 1.0f : IsKeyDown(KEY_S) ? -1.0f : IsKeyDown(KEY_DOWN) ? -1.0f : 0;

	if (moveDirection.y > 0) {	rigidBody3D.translation += (rigidBody3D.front * speed) * 0.1f;}
	if (moveDirection.y < 0) { 	rigidBody3D.translation += (rigidBody3D.back * speed) * 0.1f; }
	if (moveDirection.x < 0) { 	rigidBody3D.translation += (rigidBody3D.left * speed) * 0.1f; }
	if (moveDirection.x > 0) {	rigidBody3D.translation += (rigidBody3D.right * speed) * 0.1f;}

	if (IsKeyPressed(KEY_SPACE)) rigidBody3D.jump(20);

	//GameObject::update(deltaTime);
	
	rigidBody3D.update(deltaTime);
	
	// Clamp 2D to position from game map size to screen size
	{
		const float screenX = static_cast<float>(GetScreenWidth());
		const float screenY = static_cast<float>(GetScreenHeight());

		//rigidBody2D.translation.x = screenX / 2 + rigidBody3D.translation.x;
		//rigidBody2D.translation.y = screenY - rigidBody3D.translation.y + (rigidBody2D.scale.y * 32);

		rigidBody3D.translation.x = Clamp(rigidBody3D.translation.x, -(screenX / 2), screenX / 2);
		rigidBody3D.translation.z = Clamp(rigidBody3D.translation.z, -(screenY / 2), screenY / 2);

		//rigidBody2D.translation.x = Clamp(rigidBody2D.translation.x, 0, screenX);
		//rigidBody2D.translation.y = Clamp(rigidBody2D.translation.y, 0, screenY);

	}
	
	// Resolve Player Collision
	for (size_t i = 0; i < manager->currentScene->gameMap.gameObjects.size(); i++)
	{
		auto& object = manager->currentScene->gameMap.gameObjects[i];
		if (&object != this)
		{
			if (CheckCollisionBoxes(rigidBody3D.collisionBox, object.rigidBody3D.collisionBox))
			{
				rigidBody3D.resolveCollision(object.rigidBody3D);
			}
		}
	}

	health = Clamp(health, 0, getMaxHealth());
	stamina = Clamp(stamina, 0, getMaxStamina());

}


void PlayerCamera::UpdateCameraFPS(Camera* camera, Player* player)
{

	camera->position = Vector3(
		player->rigidBody3D.translation.x,
		player->rigidBody3D.translation.y + (player->rigidBody3D.scale.y / 2),
		player->rigidBody3D.translation.z
	);

	UpdateCamera(camera, CAMERA_FIRST_PERSON);

	// FPS-style camera look
	static float yaw = 0.0f;
	static float pitch = 0.0f;
	const float sensitivity = -0.003f;
	Vector2 mouseDelta = GetMouseDelta();
	yaw += mouseDelta.x * sensitivity;
	pitch += mouseDelta.y * sensitivity;
	if (pitch > 1.5f) pitch = 1.5f;
	if (pitch < -1.5f) pitch = -1.5f;

	Vector3 camForward = {
		cosf(pitch) * sinf(yaw),
		sinf(pitch),
		cosf(pitch) * cosf(yaw)
	};
	forward = camForward = Vector3Normalize(camForward);
	camera->target = Vector3Add(camera->position, Vector3Scale(camForward, 10.0f));

	// Update player direction vectors to match camera look
	Vector3 flatForward = camForward; flatForward.y = 0; flatForward = Vector3Normalize(flatForward);
	if (Vector3Length(flatForward) > 0.001f) {
		player->rigidBody3D.front = flatForward;
		player->rigidBody3D.back = Vector3Scale(flatForward, -1);
		player->rigidBody3D.right = Vector3{ -flatForward.z, 0, flatForward.x };
		player->rigidBody3D.left = Vector3{ flatForward.z, 0, -flatForward.x };
		player->rigidBody3D.up = Vector3{ 0, 1, 0 };
		player->rigidBody3D.down = Vector3{ 0, -1, 0 };
	}

}