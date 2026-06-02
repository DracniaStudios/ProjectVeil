#include "Player.h"

#include <raymath.h>
#include <SceneManager.h>

/// <summary>
///  Player Functions
///  
///  Enable
///  Disable
///  Render
///  Update
///  Camera
///  
/// </summary>

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
	if (!this->isEnabled) return;
	
	auto scene = SceneManager::getInstance().currentScene;
	if (scene->isMiniActive && scene->miniGame != nullptr)
	{
		DrawCircle(rigidBody2D.translation.x, rigidBody2D.translation.y, rigidBody2D.scale.x, WHITE);

	}

	/// Reticle
	DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 10, 
		IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? RED
		: IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? RED
		: WHITE);
}

void Player::render3D()
{
	if (!this->isEnabled) { return; }

	auto scene = SceneManager::getInstance().currentScene;
	if (this->displayCollider)
	{
		DrawBoundingBox(rigidBody3D->collisionBox, WHITE);
	}

	if (this->display3DModel) {
		DrawModel(*model, rigidBody3D->translation, 1.0f, Color{ 20, 30, 30, 255 });
		DrawModelWires(*model, rigidBody3D->translation, 1.0f, BLACK);
	}

	if (this->displayDirection) {
		/// Show Directions
		DrawSphere(rigidBody3D->forward + rigidBody3D->translation, 0.1f, RED);
		DrawSphere(rigidBody3D->back + rigidBody3D->translation, 0.1f, ORANGE);
		DrawSphere(rigidBody3D->left + rigidBody3D->translation, 0.1f, YELLOW);
		DrawSphere(rigidBody3D->right + rigidBody3D->translation, 0.1f, GREEN);
		DrawSphere(rigidBody3D->up + rigidBody3D->translation, 0.1f, BLUE);
		DrawSphere(rigidBody3D->down + rigidBody3D->translation, 0.1f, PURPLE);
	}

	Vector3 rayOffset = Vector3Add(camera.forward, rigidBody3D->right);
	DrawSphere(camera.position + rayOffset, 0.1f, GREEN);
	if (isFiring)	{	DrawRay(Ray{ camera.position + rayOffset, camera.forward }, GREEN);}

}

void Player::update2D(float deltaTime, bool canMove)
{
	if (!this->isEnabled) return;

	if (!SceneManager::getInstance().currentScene->is2DActive) return;

	// Player2D Function
	moveDirection = Vector2Zero();
	moveDirection.x = IsKeyDown(KEY_A) ? -1.0f : IsKeyDown(KEY_LEFT) ? -1.0f : IsKeyDown(KEY_D) ? 1.0f : IsKeyDown(KEY_RIGHT) ? 1.0f : 0;
	moveDirection.y = IsKeyDown(KEY_W) ? -1.0f : IsKeyDown(KEY_UP) ? -1.0f : IsKeyDown(KEY_S) ? 1.0f : IsKeyDown(KEY_DOWN) ? 1.0f : 0;
	
	auto speed = IsKeyDown(KEY_LEFT_SHIFT) ? baseSpeed * 2 : baseSpeed;
	
	//if (canMove) { rigidBody2D.translation += Vector3(moveDirection.x, moveDirection.y) * speed; }
	rigidBody2D.update(deltaTime);
}

void Player::update3D(float deltaTime)
{
	if (!isEnabled) return;
	if (SceneManager::getInstance().currentScene->is2DActive) { return; }

	/// Player Flags
	isCrouching = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT);

	/// Player Scale
	rigidBody3D->scale = Vector3(1, 2, 1);

	/// Player Movement
	auto speed = IsKeyDown(KEY_LEFT_SHIFT) ? baseSpeed * 2 : baseSpeed;

	// Player Movement Input
	moveDirection = Vector2Zero();
	moveDirection.x = IsKeyDown(KEY_A) ? -1.0f : IsKeyDown(KEY_LEFT) ? -1.0f : IsKeyDown(KEY_D) ? 1.0f : IsKeyDown(KEY_RIGHT) ? 1.0f : 0;
	moveDirection.y = IsKeyDown(KEY_W) ? 1.0f : IsKeyDown(KEY_UP) ? 1.0f : IsKeyDown(KEY_S) ? -1.0f : IsKeyDown(KEY_DOWN) ? -1.0f : 0;

	if (moveDirection.y > 0) {	rigidBody3D->translation += (rigidBody3D->forward * speed) * 0.1f;}
	if (moveDirection.y < 0) { 	rigidBody3D->translation += (rigidBody3D->back * speed) * 0.1f; }
	if (moveDirection.x < 0) { 	rigidBody3D->translation += (rigidBody3D->left * speed) * 0.1f; }
	if (moveDirection.x > 0) {	rigidBody3D->translation += (rigidBody3D->right * speed) * 0.1f;}

	if (IsKeyPressed(KEY_SPACE)) rigidBody3D->jump(20);

	/// Update RigidBody3D Physics
	rigidBody3D->update(SceneManager::getInstance().currentScene->gameMap, deltaTime);
	
	/// Clamp Player Position to Screen Bounds \ World Size
	const float screenX = static_cast<float>(GetScreenWidth());
	const float screenY = static_cast<float>(GetScreenHeight());

	rigidBody3D->translation.x = Clamp(rigidBody3D->translation.x, -(screenX / 2), screenX / 2);
	rigidBody3D->translation.z = Clamp(rigidBody3D->translation.z, -(screenY / 2), screenY / 2);

	/// Resolve Player Collision
	for (auto& obj : SceneManager::getInstance().currentScene->gameMap.gameObjects)
	{
		if (obj.get() != this)
		{
			if (CheckCollisionBoxes(rigidBody3D->collisionBox, obj->rigidBody3D->collisionBox))
			{
				rigidBody3D->resolveConstrains(obj->rigidBody3D);
			}
		}
	}

	/// Clamp Player Health and Stamina
	health = Clamp(health, 0, static_cast<float>(getMaxHealth()));
	stamina = Clamp(stamina, 0, static_cast<float>(getMaxStamina()));

	/// Update Player Actions
	
	isFiring = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){ Fire(); }
	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) { FireLaser(); }
}

///  Camera Update Function for First-Person Style Camera. 
///  Updates the camera's position to be at the player's head, and rotates based on mouse movement.
///  Also updates the player's direction vectors to match the camera's look direction.

void PlayerCamera::UpdateCameraFPS(Camera* camera, Player* player)
{
	offset = Vector3(0, player->rigidBody3D->scale.y / 2, 0); // Camera offset to be at player's head)
	position = Vector3Add(player->rigidBody3D->translation, offset);
	
	camera->position = Vector3Add(player->rigidBody3D->translation, offset);

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
		player->rigidBody3D->forward = flatForward;
		player->rigidBody3D->back = Vector3Scale(flatForward, -1);
		player->rigidBody3D->right = Vector3{ -flatForward.z, 0, flatForward.x };
		player->rigidBody3D->left = Vector3{ flatForward.z, 0, -flatForward.x };
		player->rigidBody3D->up = Vector3{ 0, 1, 0 };
		player->rigidBody3D->down = Vector3{ 0, -1, 0 };
	}

}

// Projectile Variant
void Player::Fire()
{
	Entity* projectile = new Entity();
	projectile->name = "Projectile";
	projectile->type = OBJECT_PROJECTILE;
	projectile->baseDamage = baseDamage * 0.5f;
	projectile->baseSpeed = 10;

	projectile->rigidBody3D->translation = rigidBody3D->translation + camera.forward;
	projectile->rigidBody3D->scale = Vector3(0.2f, 0.2f, 0.2f);
	projectile->meshVariant = MESH_CUBE;

	projectile->rigidBody3D->velocity = Vector3Scale(camera.forward, projectile->baseSpeed * 10);

	projectile->Decay(3);
	projectile->health = -1;
	projectile->isAlive = false;

	GameObject* obj = projectile;
	auto& bullet = SceneManager::getInstance().currentScene->gameMap.saveObjectAt(projectile->rigidBody3D->translation, *obj);

}

void Player::FireLaser()
{
	// Create Ray from Camera
	Ray cameraRay = {camera.position, camera.forward};

	// Check Ray Collision with Game Objects
	
	for (auto& obj : SceneManager::getInstance().currentScene->gameMap.gameObjects)
	{
		auto collider = GetRayCollisionBox(cameraRay, obj->rigidBody3D->collisionBox);

		// If Collision, Apply Damage to Object
		if (collider.hit)
		{
			obj->onHit(this);
			obj->rigidBody3D->addForce(camera.forward, 10.0f);
		}
	}
}
