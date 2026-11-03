#include "Player.h"

#include <raymath.h>
#include <LightingSystem.h>
#include <SceneManager.h>
#include <WorldEditor.h>

#include <cmath>

// !!! Warning: Emit Functions Counts on Player Translation and Move Direction instead of Velocity !!!

/* Static Functions */
#pragma region Static Functions
void SetMoveDirection(Player* player, const Sim::PlayerCommand& command, float deltaTime) {

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

	// Player Movement Input, already reduced to -1, 0 or +1 per axis
	player->moveDirection = Vector2Zero();
	player->moveDirection.x = command.moveX < 0 ? -1.0f : command.moveX > 0 ? 1.0f : 0.0f;
	player->moveDirection.y = command.moveY < 0 ? -1.0f : command.moveY > 0 ? 1.0f : 0.0f;

	// Movement was applied as a flat `speed * 0.1f` per frame, so how fast the
	// player walked was decided by the monitor: identical inputs covered 2.4x
	// the distance at 144 FPS that they did at 60. Scaling by deltaTime and
	// multiplying back up by 60 keeps the tuning that was authored against the
	// 60 FPS SetTargetFPS default. With the fixed tick, deltaTime is always
	// Sim::kTickDelta, so this is one 60th of the authored per-second speed.
	const float step = speed * 0.1f * 60.0f * deltaTime;

	if (player->moveDirection.y > 0) { player->rigidBody3D.translation += player->rigidBody3D.forward * step; }
	if (player->moveDirection.y < 0) { player->rigidBody3D.translation += player->rigidBody3D.back * step; }
	if (player->moveDirection.x < 0) { player->rigidBody3D.translation += player->rigidBody3D.left * step; }
	if (player->moveDirection.x > 0) { player->rigidBody3D.translation += player->rigidBody3D.right * step; }
}

void UpdateActions(Player* player, SceneManagement::Scene& scene, const Sim::PlayerCommand& command) {

	/// Player Flags
	player->isCrouching = Sim::HasButton(command, Sim::BUTTON_CROUCH);
	player->isSprinting = Sim::HasButton(command, Sim::BUTTON_SPRINT);
	player->isFiring = Sim::HasButton(command, Sim::BUTTON_USE_ITEM)
		|| Sim::HasButton(command, Sim::BUTTON_USE_ITEM2);

	// The Pause overlay is the client's to open (Scene_frame): the simulation
	// never touches an overlay. The World Editor's suppression is the client's
	// too: while it is open, the client sends commands with nothing pressed.
	if (Sim::HasButton(command, Sim::BUTTON_INTERACT)) { player->Interact(scene); }
	if (Sim::HasButton(command, Sim::BUTTON_JUMP)) player->rigidBody3D.Jump(20);

	// The player's own switch. Lighting draws whichever player is local
	// (Scene_frame), so the simulation never reaches into the renderer.
	if (Sim::HasButton(command, Sim::BUTTON_FLASHLIGHT)) { player->flashlightOn = !player->flashlightOn; }
}

void updateArtifact(Player* player, SceneManagement::Scene& scene, const Sim::PlayerCommand& command, float deltaTime) {

	// Artifact Actions
	player->artifactMode += Sim::HasButton(command, Sim::BUTTON_ARTIFACT_R) ? 1
		: Sim::HasButton(command, Sim::BUTTON_ARTIFACT_L) ? -1
		: 0;
	player->artifactMode = static_cast<int>(Clamp(static_cast<float>(player->artifactMode), -1, player->artifactUnlocked));
	
	/*
	// A Practice run: the game's buff, but no station completed and no Target toggled
	if (!scene.miniGameSession.IsRunning() && Sim::HasButton(command, Sim::BUTTON_ARTIFACT))
	{
		scene.StartMiniGame(player->artifactMode);
	}
	*/

	// Artifact, held in front of the eye and to the left
	const auto camera = player->camera;
	const auto offset = Vector3Add(camera.forward, player->rigidBody3D.left);//camera->left / 2);

	player->artifact->rigidBody3D.translation = Vector3Add(player->EyePosition(), offset);
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
	const Vector3 velocity =  Vector3(player->moveDirection.x, 0, player->moveDirection.y);
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
	if (player->isCrouching) { return; }
	else if (player->isSprinting) { interval = 0.28f; loudness = 1.0f; }

	player->footstepTimer -= deltaTime;
	if (player->footstepTimer > 0.0f) { return; }
	player->footstepTimer = interval;

	scene.soundField.Emit(player->getPosition(), loudness, SOUND_FOOTSTEP, player->id);
}

void Player::ApplyLook(float yawDelta, float pitchDelta)
{
	// Never trust the size of a turn: a NaN or a huge delta would leave the
	// player looking nowhere (rule P-6 clamps every delta to a half turn).
	constexpr float kHalfTurn = 3.14159265358979323846f;
	const auto clampDelta = [](float delta) {
		if (!std::isfinite(delta)) { return 0.0f; }
		return Clamp(delta, -kHalfTurn, kHalfTurn);
	};

	lookYaw = std::remainder(lookYaw + clampDelta(yawDelta), 2.0f * kHalfTurn);
	lookPitch = Clamp(lookPitch + clampDelta(pitchDelta), -kPitchLimit, kPitchLimit);
	camera.SetLook(lookYaw, lookPitch, rigidBody3D);
}

void Player::SetLookAngles(float yaw, float pitch)
{
	lookYaw = 0.0f;
	lookPitch = 0.0f;
	ApplyLook(yaw, pitch);
}

Vector3 Player::EyePosition() const
{
	// Half the body's height above its centre; crouching halves that again
	Vector3 offset = Vector3(0, rigidBody3D.scale.y / 2, 0);
	if (isCrouching) { offset.y /= 2; }
	return Vector3Add(rigidBody3D.translation, offset);
}

void Player::update3D(SceneManagement::Scene& scene, const Sim::PlayerCommand& command, float deltaTime)
{
	if (!isEnabled) return;

	/// Player Scale
	rigidBody3D.scale = Vector3(1, 2, 1);

	// Look first, so this tick's movement goes the way the player now faces
	ApplyLook(command.yawDelta, command.pitchDelta);

	UpdateActions(this, scene, command);

	SetMoveDirection(this, command, deltaTime);

	/// Update RigidBody3D Physics. Its collisions come afterwards, from
	/// Scene::SolveCollisions over every body at once.
	rigidBody3D.Update(deltaTime);

	// After the physics step so the noise is stamped at the position the player
	// actually reached this frame, not the one they started it at.
	EmitFootsteps(this, scene, deltaTime);

	camera.position = EyePosition();

	if (artifact) { updateArtifact(this, scene, command, deltaTime); }

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