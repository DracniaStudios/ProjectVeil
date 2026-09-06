#include "Player.h"

#include <raymath.h>
#include <LightingSystem.h>
#include <SceneManager.h>
#include <WorldEditor.h>

/* Static Functions */
#pragma region Static Functions
void SetMoveDirection(Player* player, float deltaTime) {
	auto inputSystem = &InputSystem::getInstance();

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
	player->moveDirection.x = inputSystem->IsActionDown(ACTION_MOVE_LEFT) ? -1.0 :
		inputSystem->IsActionDown(ACTION_MOVE_RIGHT) ? 1.0f : 0;
	player->moveDirection.y = inputSystem->IsActionDown(ACTION_MOVE_FORWARD) ? 1.0 :
		inputSystem->IsActionDown(ACTION_MOVE_BACKWARD) ? -1.0f : 0;

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

void UpdateActions(Player* player) {
	auto inputSystem = &InputSystem::getInstance();

	/// Player Flags
	player->isCrouching = inputSystem->IsActionDown(ACTION_MOVE_CROUCH);
	player->isSprinting = inputSystem->IsActionDown(ACTION_MOVE_SPRINT);
	player->isFiring = inputSystem->IsActionPressed(ACTION_USE_ITEM)
		|| inputSystem->IsActionDown(ACTION_USE_ITEM2);

	if (!WorldEditor::getInstance().IsEnabled()) {
		if (inputSystem->IsActionPressed(ACTION_MOVE_INTERACT)) { player->Interact(); }
		if (inputSystem->IsActionPressed(ACTION_MOVE_JUMP)) player->rigidBody3D.Jump(20);

		// The flashlight is owned by LightingSystem and follows whichever camera
		// is active, so the player only owns the toggle. Sitting inside this
		// block keeps it suppressed while the World Editor has focus, matching
		// fire/interact/jump.
		if (inputSystem->IsActionPressed(ACTION_USE_FLASHLIGHT)) { LightingSystem::getInstance().ToggleFlashlight(); }
	}
}

void updateArtifact(Player* player, float deltaTime) {

	auto inputSystem = &InputSystem::getInstance();
	const auto scene = SceneManager::getInstance().currentScene;

	// Artifact Actions
	player->artifactMode += inputSystem->IsActionPressed(ACTION_USE_ARTIFACT_RIGHT) ? 1
		: inputSystem->IsActionPressed(ACTION_USE_ARTIFACT_LEFT) ? -1
		: 0;
	player->artifactMode = static_cast<int>(Clamp(static_cast<float>(player->artifactMode), -1, player->artifactUnlocked));
	// ACTION_USE_ARTIFACT and ACTION_MOVE_INTERACT are both bound to F (see
	// InputSystem::SetDefaultActions), and UpdateActions runs Interact() earlier
	// in this same frame — so one keypress reached SetMiniGame twice, and the
	// game the player actually walked up to and interacted with was immediately
	// torn down and replaced by whatever the artifact dial was pointing at.
	// Whichever launch ran first wins; the second is dropped rather than
	// silently overriding it.
	if (scene->miniGame == nullptr && inputSystem->IsActionPressed(ACTION_USE_ARTIFACT))
	{
		scene->SetMiniGame(player->artifactMode);
	}

	// Artifact
	const auto camera = player->camera;
	const auto offset = Vector3Add(camera.forward, player->rigidBody3D.left);//camera->left / 2);

	player->artifact->rigidBody3D.translation = Vector3Add(SceneManager::getInstance().camera3D.position, offset);
	player->artifact->update(scene, deltaTime);
}

#pragma endregion

void Player::onCollision(const GameObject* collider)
{
	Entity::onCollision(collider);

	if (collider == nullptr || collider->type != OBJECT_ENTITY) { return; }

	// Narrowed by `type` first, so the cast below only ever runs on something
	// that really is an Entity — the same checked-cast discipline the physics
	// solver uses before calling type-specific reactions.
	const auto* entity = static_cast<const Entity*>(collider);
	if (entity->kind != ENTITYKIND_STALKER) { return; }

	// Caught. Contact damage rather than Entity::Attack(), which spawns a
	// projectile — wrong shape for an enemy whose whole threat is reaching you.
	applyHealthValue(entity->baseDamage, true);
}

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
	auto inputSystem = &InputSystem::getInstance();
	auto scene = SceneManager::getInstance().currentScene;

	if (scene->isMiniActive && scene->miniGame != nullptr)
	{
		DrawCircle(rigidBody2D.translation.x, rigidBody2D.translation.y, rigidBody2D.scale.x, WHITE);
	}

	/// Reticle
	if (!scene->is2DActive) {
		DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 10,
			inputSystem->IsActionDown(ACTION_USE_ITEM) ? RED
			: inputSystem->IsActionDown(ACTION_USE_ITEM2) ? RED
			: WHITE);
	}
}

void Player::render3D()
{
	if (!this->isEnabled) { return; }
	if (artifact) { artifact->render3D(); }
	GameObject::render3D();
}

void Player::update2D(float deltaTime, bool canMove)
{
	if (!this->isEnabled) return;
	if (!SceneManager::getInstance().currentScene->is2DActive) return;

	auto inputSystem = &InputSystem::getInstance();

	// Player2D Function
	moveDirection = Vector2Zero();
	moveDirection.x = inputSystem->IsActionDown(ACTION_MOVE_LEFT) ? -1.0 : inputSystem->IsActionDown(ACTION_MOVE_RIGHT) ? 1.0f : 0;
	moveDirection.y = inputSystem->IsActionDown(ACTION_MOVE_FORWARD) ? 1.0 : inputSystem->IsActionDown(ACTION_MOVE_BACKWARD) ? -1.0f : 0;

	auto speed = inputSystem->IsActionDown(ACTION_MOVE_SPRINT) ? baseSpeed * 2 : baseSpeed;

	// moveDirection/speed used to be computed here and then thrown away — this
	// left is2DActive mode (toggled with TAB) with movement keys that read
	// input but never moved the player. Applied the same way SetMoveDirection
	// does for the 3D player, scaled by deltaTime so it isn't tied to frame rate.
	if (canMove)
	{
		rigidBody2D.translation += Vector3(moveDirection.x, moveDirection.y) * speed * 60.0f * deltaTime;
	}

	rigidBody2D.update(deltaTime);
}

// Footsteps, the player's loudest routine emission.
//
// Cadence and loudness both key off gait, which is what makes crouch-walking a
// real tactic rather than a cosmetic one: it is quieter per step *and* emits
// fewer steps. Loudness values match the plan's table (crouch .15 / walk .5 /
// sprint 1.0).
static void EmitFootsteps(Player* player, float deltaTime)
{
	auto scene = SceneManager::getInstance().currentScene;
	if (scene == nullptr) { return; }

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

	scene->soundField.Emit(player->getPosition(), loudness, SOUND_FOOTSTEP, player->id);
}

void Player::update3D(float deltaTime)
{
	if (!isEnabled) return;
	if (SceneManager::getInstance().currentScene->is2DActive) { return; }

	/// Player Scale
	rigidBody3D.scale = Vector3(1, 2, 1);

	UpdateActions(this);

	SetMoveDirection(this, deltaTime);

	// The player ticks its own body rather than going through GameObject::update,
	// so it needs its own trigger dispatch or it would never report leaving one.
	// currentScene is null between the two halves of a scene transition.
	if (Scene* scene = SceneManager::getInstance().currentScene)
	{
		rigidBody3D.DispatchTriggerEvents(this, &scene->gameMap);
	}

	/// Update RigidBody3D Physics
	rigidBody3D.Update(deltaTime);

	// After the physics step so the noise is stamped at the position the player
	// actually reached this frame, not the one they started it at.
	EmitFootsteps(this, deltaTime);

	if (artifact) { updateArtifact(this, deltaTime); }

	stamina = Clamp(stamina, 0, getMaxStamina());
	health = Clamp(health, 0, getMaxHealth());
}

FMOD_3D_ATTRIBUTES Player::getListener() {
	const auto camera = &SceneManager::getInstance().currentScene->player->camera;

	// FMOD requires forward and up to be normalized and perpendicular
	Vector3 forward = Vector3Normalize(camera->forward);
	if (Vector3LengthSqr(forward) < 0.0001f) { forward = Vector3(0, 0, 1); }
	Vector3 right = Vector3CrossProduct(Vector3(0, -1, 0), forward);
	if (Vector3LengthSqr(right) < 0.0001f) { right = Vector3(1, 0, 0); } // looking straight up/down
	Vector3 up = Vector3Normalize(Vector3CrossProduct(forward, right));

	FMOD_3D_ATTRIBUTES listenerAttributes = {};
	listenerAttributes.position = Vector3ToFMOD(SceneManager::getInstance().camera3D.position);
	listenerAttributes.velocity = Vector3ToFMOD(getVelocity());
	listenerAttributes.forward = Vector3ToFMOD(forward);
	listenerAttributes.up = Vector3ToFMOD(up);
	return listenerAttributes;
}