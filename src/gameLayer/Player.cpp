#include "Player.h"

#include <raymath.h>
#include <SceneManager.h>


void Player::onEnable()
{
	GameObject::onEnable();
	/*
	isEnabled = true;
	rigidBody2D.scale = Vector3(1, 1, 1);
	rigidBody3D.scale = Vector3(1, 1, 1);

	// Generate 3D Model
	mesh = GenMeshCube(rigidBody3D.scale.x, rigidBody3D.scale.y, rigidBody3D.scale.z);
	model = LoadModelFromMesh(mesh);

	// Generate 2d Model

	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(mesh);
	*/
}

void Player::onDisable()
{
	GameObject::onDisable();
	/*
	isEnabled = false;
	UnloadModel(model);
	UnloadMesh(mesh);
	*/
}

void Player::render2D()
{
	if (!isEnabled) { return; }
	Rectangle pos2D = { rigidBody2D.translation.x - rigidBody2D.scale.x / 2,
		rigidBody2D.translation.y - rigidBody2D.scale.y / 2,
		rigidBody3D.scale.x * 32,
		rigidBody3D.scale.y * 32 };

	pos2D.y -= rigidBody3D.scale.y * 32;

	if (display2DModel) {
		DrawRectangle(pos2D.x, pos2D.y, pos2D.width, pos2D.height, defaultColor);
	}
}

void Player::render3D()
{
	if (!isEnabled) { return; }

	DrawSphere(rigidBody2D.translation, 0.5f, RED);


	if (displayCollider) { DrawBoundingBox(rigidBody3D.collisionBox, WHITE); }

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

void Player::update(SceneManager* manager, float deltaTime)
{
	if (!isEnabled) return;

	/// Player Flags
	isCrouching = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT);

	/// Player Scale
	rigidBody3D.scale = Vector3(1, 2, 1);

	/// Player Movement
	auto speed = IsKeyDown(KEY_LEFT_SHIFT) ? baseSpeed * 2 : baseSpeed;

	if (IsKeyDown(KEY_W)) { 
		rigidBody3D.translation += (rigidBody3D.front * speed) * 0.1f;
	}
	if (IsKeyDown(KEY_S)) { 
		rigidBody3D.translation += (rigidBody3D.back * speed) * 0.1f;
	}
	if (IsKeyDown(KEY_A)) { 
		rigidBody3D.translation += (rigidBody3D.left * speed) * 0.1f;
	}
	if (IsKeyDown(KEY_D)) {
		rigidBody3D.translation += (rigidBody3D.right * speed) * 0.1f;
	}

	if (IsKeyPressed(KEY_SPACE)) rigidBody3D.jump(20);

	//GameObject::update(deltaTime);
	
	rigidBody3D.update(deltaTime);
	
	// Clamp 2D to position from game map size to screen size
	{
		const float screenX = static_cast<float>(GetScreenWidth());
		const float screenY = static_cast<float>(GetScreenHeight());

		rigidBody2D.translation.x = screenX / 2 + rigidBody3D.translation.x;
		rigidBody2D.translation.y = screenY - rigidBody3D.translation.y + (rigidBody2D.scale.y * 32);

		rigidBody3D.translation.x = Clamp(rigidBody3D.translation.x, -(screenX / 2), screenX / 2);
		rigidBody3D.translation.z = Clamp(rigidBody3D.translation.z, -(screenY / 2), screenY / 2);

		rigidBody2D.translation.x = Clamp(rigidBody2D.translation.x, 0, screenX);
		rigidBody2D.translation.y = Clamp(rigidBody2D.translation.y, 0, screenY);

	}
	
	for (size_t i = 0; i < manager->currentScene->gameMap.gameObjects.size(); i++)
	{
		auto object = manager->currentScene->gameMap.gameObjects[i];
		if (object != this)
		{
			if (CheckCollisionBoxes(rigidBody3D.collisionBox, object->rigidBody3D.collisionBox))
			{
				rigidBody3D.resolveCollision(object->rigidBody3D);
			}
		}
	}

}