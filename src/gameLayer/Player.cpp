#include "Player.h"

#include <raymath.h>
#include <SceneManager.h>
#include <WorldEditor.h>

/* Static Functions */
#pragma region Static Functions
void SetMoveDirection(Player* player) {
	/// Player Movement
	auto& speed = player->currentSpeed;
	speed = player->isSprinting ? player->baseSpeed * 2 :
		player->isCrouching ? player->baseSpeed / 2 : player->baseSpeed;

	auto& jumpPower = player->currentJumpPower;

	if (auto* movementBuff = player->getBuff(BUFF_MOVEMENT);
		movementBuff && movementBuff->remaining_time() > 0)
	{
		speed *= 2; jumpPower += player->baseJumpPower * 2;
	}

	// Player Movement Input
	player->moveDirection = Vector2Zero();
	player->moveDirection.x = InputSystem::getInstance().IsActionDown(ACTION_MOVE_LEFT) ? -1.0 :
		InputSystem::getInstance().IsActionDown(ACTION_MOVE_RIGHT) ? 1.0f : 0;
	player->moveDirection.y = InputSystem::getInstance().IsActionDown(ACTION_MOVE_FORWARD) ? 1.0 :
		InputSystem::getInstance().IsActionDown(ACTION_MOVE_BACKWARD) ? -1.0f : 0;

	if (player->moveDirection.y > 0) { player->rigidBody3D.translation += (player->rigidBody3D.forward * speed) * 0.1f; }
	if (player->moveDirection.y < 0) { player->rigidBody3D.translation += (player->rigidBody3D.back * speed) * 0.1f; }
	if (player->moveDirection.x < 0) { player->rigidBody3D.translation += (player->rigidBody3D.left * speed) * 0.1f; }
	if (player->moveDirection.x > 0) { player->rigidBody3D.translation += (player->rigidBody3D.right * speed) * 0.1f; }
}

void UpdateActions(Player* player) {
	/// Player Flags
	player->isCrouching = InputSystem::getInstance().IsActionDown(ACTION_MOVE_CROUCH);
	player->isSprinting = InputSystem::getInstance().IsActionDown(ACTION_MOVE_SPRINT);
	player->isFiring = InputSystem::getInstance().IsActionPressed(ACTION_USE_ITEM)
		|| InputSystem::getInstance().IsActionDown(ACTION_USE_ITEM2);

	if (!WorldEditor::getInstance().IsEnabled()) {
		if (InputSystem::getInstance().IsActionPressed(ACTION_USE_ITEM)) { player->Fire(); }
		if (InputSystem::getInstance().IsActionDown(ACTION_USE_ITEM2)) { player->FireLaser(); }
	}

	if (InputSystem::getInstance().IsActionPressed(ACTION_MOVE_INTERACT)) { player->Interact(); }

	if (InputSystem::getInstance().IsActionPressed(ACTION_MOVE_JUMP)) player->rigidBody3D.Jump(20);
}

void updateCollision(Player* player) {
	/// Resolve Player Collision
	for (auto& obj : SceneManager::getInstance().currentScene->gameMap.gameObjects)
	{
		if (&obj != player)
		{
			if (CheckCollisionBoxes(player->rigidBody3D.collisionBox, obj.rigidBody3D.collisionBox))
			{
				player->rigidBody3D.resolveConstrains(player, &obj);
			}
		}
	}
}

void updateArtifact(Player* player) {
	
	auto scene = SceneManager::getInstance().currentScene;

	// Artifact Actions
	player->artifactMode += InputSystem::getInstance().IsActionPressed(ACTION_USE_ARTIFACT_RIGHT) ? 1
		: InputSystem::getInstance().IsActionPressed(ACTION_USE_ARTIFACT_LEFT) ? -1
		: 0;
	player->artifactMode = static_cast<int>(Clamp(static_cast<float>(player->artifactMode), 0, 5));

	if (InputSystem::getInstance().IsActionPressed(ACTION_USE_ARTIFACT)) { scene->SetMiniGame(player->artifactMode); };

	
	// Artifact
	auto camera = scene->camera;
	auto offset = Vector3Add(camera->forward, player->rigidBody3D.left);//camera->left / 2);
	player->artifact->rigidBody3D.translation = Vector3Add(camera->position, offset);
	player->artifact->rigidBody3D.angularVelocity = Vector3(0.1f, 0, 0.1f);
	player->artifact->update(scene, GetFrameTime());
	
	
	// All Sides need to face the player, these are direct references for current position
	// More Animatin than mechanic
	// Quaternion Red = {0, 0.3, 0, 1}
	// Quaternion Orange = {0, 0.95, 0, -0.3}
	// Quaternion Green = {0, 0.87, 0, 0.47}
	// Quaternion Blue = {0, -0.4, 0, 1}
	// Quaternion White = {0.65, 0.2, -0.2, 0.7}
	// Quaternion Yellow = {-0.68, 0.2, 0.2, 0.7}
}

#pragma endregion

void Player::onEnable()
{
	maxHealth = 100.0f;

	id = PLAYER_ID;
	rigidBody2D.translation = Vector3(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f, 0);
	rng = std::ranlux24_base(std::random_device{}());
	display3DModel = false;
	
	Entity::onEnable();
	
	// Override Entity Buffs
	//buffTimers.clear();

}

void Player::onDisable()
{
	Entity::onDisable();
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
	if (!scene->is2DActive) {
		DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 10,
			InputSystem::getInstance().IsActionDown(ACTION_USE_ITEM) ? RED
			: InputSystem::getInstance().IsActionDown(ACTION_USE_ITEM2) ? RED
			: WHITE);
	}
}

void Player::render3D()
{
	if (!this->isEnabled) { return; }
	artifact->render3D();
	GameObject::render3D();
}

void Player::update2D(float deltaTime, bool canMove)
{
	if (!this->isEnabled) return;

	if (!SceneManager::getInstance().currentScene->is2DActive) return;

	// Player2D Function
	moveDirection = Vector2Zero();
	moveDirection.x = InputSystem::getInstance().IsActionDown(ACTION_MOVE_LEFT) ? -1.0 : InputSystem::getInstance().IsActionDown(ACTION_MOVE_RIGHT) ? 1.0f : 0;
	moveDirection.y = InputSystem::getInstance().IsActionDown(ACTION_MOVE_FORWARD) ? 1.0 : InputSystem::getInstance().IsActionDown(ACTION_MOVE_BACKWARD) ? -1.0f : 0;
	
	auto speed = InputSystem::getInstance().IsActionDown(ACTION_MOVE_SPRINT) ? baseSpeed * 2 : baseSpeed;
	
	rigidBody2D.update(deltaTime);
}

void Player::update3D(float deltaTime)
{
	if (!isEnabled) return;
	if (SceneManager::getInstance().currentScene->is2DActive) { return; }

	/// Player Scale
	rigidBody3D.scale = Vector3(1, 2, 1);

	UpdateActions(this);
	
	SetMoveDirection(this);

	/// Update RigidBody3D Physics
	rigidBody3D.Update(deltaTime);
	
	updateCollision(this);

	updateArtifact(this);

	stamina = Clamp(stamina, 0, getMaxStamina());
	health = Clamp(health, 0, getMaxStamina());
}

FMOD_3D_ATTRIBUTES Player::getListener() {
	const auto camera = SceneManager::getInstance().currentScene->camera;

	// FMOD requires forward and up to be normalized and perpendicular
	Vector3 forward = Vector3Normalize(camera->forward);
	if (Vector3LengthSqr(forward) < 0.0001f) { forward = Vector3(0, 0, 1); }
	Vector3 right = Vector3CrossProduct(Vector3(0, -1, 0), forward);
	if (Vector3LengthSqr(right) < 0.0001f) { right = Vector3(1, 0, 0); } // looking straight up/down
	Vector3 up = Vector3Normalize(Vector3CrossProduct(forward, right));

	FMOD_3D_ATTRIBUTES listenerAttributes = {};
	listenerAttributes.position = Vector3ToFMOD(camera->position);
	listenerAttributes.velocity = Vector3ToFMOD(getVelocity());
	listenerAttributes.forward = Vector3ToFMOD(forward);
	listenerAttributes.up = Vector3ToFMOD(up);
	return listenerAttributes;
}