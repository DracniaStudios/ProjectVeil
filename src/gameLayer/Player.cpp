#include "Player.h"

#include <raymath.h>
#include <LightingSystem.h>
#include <SceneManager.h>
#include <WorldEditor.h>

/* Static Functions */
#pragma region Static Functions
void SetMoveDirection(Player* player, float deltaTime) {
	auto inputSystem = &InputSystem::InputSystem::getInstance();

	/// Player Movement
	auto& speed = player->currentSpeed;
	speed = player->isSprinting ? player->baseSpeed * 2 :
		player->isCrouching ? player->baseSpeed / 2 : player->baseSpeed;

	auto& jumpPower = player->currentJumpPower;
	jumpPower = player->isSprinting ? player->baseJumpPower * 1.5f :
		player->isCrouching ? player->baseJumpPower / 2 : player->baseJumpPower;

	if (auto* movementBuff = player->getBuff(BUFF_MOVEMENT);
		movementBuff && movementBuff->remaining_time() > 0)
	{
		speed *= 2; 
		jumpPower *= 2;
	}

	// Player Movement Input
	player->moveDirection = Vector2Zero();
	player->moveDirection.x = inputSystem->IsActionDown(InputSystem::ACTION_MOVE_LEFT) ? -1.0 :
		inputSystem->IsActionDown(InputSystem::ACTION_MOVE_RIGHT) ? 1.0f : 0;
	player->moveDirection.y = inputSystem->IsActionDown(InputSystem::ACTION_MOVE_FORWARD) ? 1.0 :
		inputSystem->IsActionDown(InputSystem::ACTION_MOVE_BACKWARD) ? -1.0f : 0;

	// Movement was applied as a flat `speed * 0.1f` per frame, so how fast the
	// player walked was decided by the monitor: identical inputs covered 2.4x
	// the distance at 144 FPS that they did at 60. Scaling by deltaTime and
	// multiplying back up by 60 keeps the tuning that was authored against the
	// 60 FPS SetTargetFPS default while making it frame-rate independent — the
	// same correction Player::update2D already carries.
	const float step = speed * 0.1f * 60.0f * deltaTime;

	if (player->moveDirection.y > 0) { player->rigidBody3D.translation += player->rigidBody3D.forward * step; }
	if (player->moveDirection.y < 0) { player->rigidBody3D.translation += player->rigidBody3D.back * step; }
	if (player->moveDirection.x < 0) { player->rigidBody3D.translation += player->rigidBody3D.left * step; }
	if (player->moveDirection.x > 0) { player->rigidBody3D.translation += player->rigidBody3D.right * step; }
}

void UpdateActions(Player* player, SceneManagement::Scene& scene) {
	auto inputSystem = &InputSystem::InputSystem::getInstance();

	/// Player Flags
	player->isCrouching = inputSystem->IsActionDown(InputSystem::ACTION_MOVE_CROUCH);
	player->isSprinting = inputSystem->IsActionDown(InputSystem::ACTION_MOVE_SPRINT);
	player->isFiring = inputSystem->IsActionPressed(InputSystem::ACTION_USE_ITEM)
		|| inputSystem->IsActionDown(InputSystem::ACTION_USE_ITEM2);

	if (!Editor::WorldEditor::getInstance().IsEnabled()) {
		if (inputSystem->IsActionPressed(InputSystem::ACTION_MOVE_INTERACT)) { player->Interact(scene); }
		if (inputSystem->IsActionPressed(InputSystem::ACTION_MOVE_JUMP)) player->rigidBody3D.Jump(20);

		// The flashlight is owned by LightingSystem and follows whichever camera
		// is active, so the player only owns the toggle. Sitting inside this
		// block keeps it suppressed while the World Editor has focus, matching
		// fire/interact/jump.
		if (inputSystem->IsActionPressed(InputSystem::ACTION_USE_FLASHLIGHT)) { Lighting::LightingSystem::getInstance().ToggleFlashlight(); }
	}
}

void updateArtifact(Player* player, SceneManagement::Scene& scene, float deltaTime) {

	auto inputSystem = &InputSystem::InputSystem::getInstance();

	// Artifact Actions
	player->artifactMode += inputSystem->IsActionPressed(InputSystem::ACTION_USE_ARTIFACT_RIGHT) ? 1
		: inputSystem->IsActionPressed(InputSystem::ACTION_USE_ARTIFACT_LEFT) ? -1
		: 0;
	player->artifactMode = static_cast<int>(Clamp(static_cast<float>(player->artifactMode), -1, player->artifactUnlocked));
	
	/*
	// A Practice run: the game's buff, but no station completed and no Target toggled
	if (!scene.miniGameSession.IsRunning() && inputSystem->IsActionPressed(InputSystem::ACTION_USE_ARTIFACT))
	{
		scene.StartMiniGame(player->artifactMode);
	}
	*/

	// Artifact
	const auto camera = player->camera;
	const auto offset = Vector3Add(camera.forward, player->rigidBody3D.left);//camera->left / 2);

	player->artifact->rigidBody3D.translation = Vector3Add(SceneManagement::SceneManager::getInstance().camera3D.position, offset);
	player->artifact->update(&scene, deltaTime);
}

#pragma endregion

void Player::onEnable()
{
	maxHealth = 100.0f;

	id = PLAYER_ID;
	rng =std::ranlux24_base(std::random_device{}());
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
	auto inputSystem = &InputSystem::InputSystem::getInstance();

	/// Reticle
	DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 10,
		inputSystem->IsActionDown(InputSystem::ACTION_USE_ITEM) ? RED
		: inputSystem->IsActionDown(InputSystem::ACTION_USE_ITEM2) ? RED
		: WHITE);
}

void Player::render3D()
{
	if (!this->isEnabled) { return; }
	if (artifact) { artifact->render3D(); }
	Entity::render3D();
}

// Footsteps, the player's loudest routine emission.
static void EmitFootsteps(Player* player, SceneManagement::Scene& scene, float deltaTime)
{
	// Vertical motion is jumping and falling, not walking; only horizontal
	// travel makes footfalls.
	const Vector3 velocity = player->rigidBody3D.GetVelocity();
	const float planarSpeed = Vector2Length(Vector2{ velocity.x, velocity.z });

	constexpr float kMovingThreshold = 0.5f;
	if (planarSpeed < kMovingThreshold)
	{
		// Land the next step immediately on moving off again, so starting to
		// walk is audible rather than free for the first half-stride.
		player->footstepTimer = 0.0f;
		return;
	}

	float interval = 0.45f;
	float loudness = 0.5f;
	if (player->isCrouching)      { interval = 0.70f; loudness = 0.15f; }
	else if (player->isSprinting) { interval = 0.28f; loudness = 1.0f; }

	player->footstepTimer -= deltaTime;
	if (player->footstepTimer > 0.0f) { return; }
	player->footstepTimer = interval;

	scene.soundField.Emit(player->getPosition(), loudness, SOUND_FOOTSTEP, player->id);
}

void Player::update3D(SceneManagement::Scene& scene, float deltaTime)
{
	if (!isEnabled) return;
	if (!scene.overlays.GameplayHasInput()) { return; }

	/// Player Scale
	rigidBody3D.scale = Vector3(1, 2, 1);

	UpdateActions(this, scene);

	SetMoveDirection(this, deltaTime);

	/// Update RigidBody3D Physics. Its collisions come afterwards, from
	/// Scene::SolveCollisions over every body at once.
	rigidBody3D.Update(deltaTime);

	// After the physics step so the noise is stamped at the position the player
	// actually reached this frame, not the one they started it at.
	EmitFootsteps(this, scene, deltaTime);

	if (artifact) { updateArtifact(this, scene, deltaTime); }

	stamina = Clamp(stamina, 0, getMaxStamina());
	health = Clamp(health, 0, getMaxHealth());
}

FMOD_3D_ATTRIBUTES Player::getListener() {
	const auto camera = &this->camera;

	// FMOD requires forward and up to be normalized and perpendicular
	Vector3 forward = Vector3Normalize(camera->forward);
	if (Vector3LengthSqr(forward) < 0.0001f) { forward = Vector3(0, 0, 1); }
	Vector3 right = Vector3CrossProduct(Vector3(0, -1, 0), forward);
	if (Vector3LengthSqr(right) < 0.0001f) { right = Vector3(1, 0, 0); } // looking straight up/down
	Vector3 up = Vector3Normalize(Vector3CrossProduct(forward, right));

	FMOD_3D_ATTRIBUTES listenerAttributes = {};
	listenerAttributes.position = AudioManager::Vector3ToFMOD(SceneManagement::SceneManager::getInstance().camera3D.position);
	listenerAttributes.velocity = AudioManager::Vector3ToFMOD(getVelocity());
	listenerAttributes.forward = AudioManager::Vector3ToFMOD(forward);
	listenerAttributes.up = AudioManager::Vector3ToFMOD(up);
	return listenerAttributes;
}