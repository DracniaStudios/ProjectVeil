#include <Player.h>

#include <SceneManager.h>
///  Camera Update Function for First-Person Style Camera. 
///  Updates the camera's position to be at the player's head, and rotates based on mouse movement.
///  Also updates the player's direction vectors to match the camera's look direction.

void PlayerCamera::UpdateCameraFPS(Camera3D* camera)
{
	auto player = SceneManager::getInstance().currentScene->player;

	auto up = Vector3(0.0f, 1.0f, 0.0f);
	offset = Vector3(0, player->rigidBody3D.scale.y / 2, 0); // Camera offset to be at player's head

	if (InputSystem::getInstance().IsActionDown(ACTION_MOVE_CROUCH)) { offset.y /= 2; }

	camera->position = Vector3Add(player->rigidBody3D.translation, offset);

	UpdateCamera(camera, CAMERA_CUSTOM);

	// FPS-style camera look. lookRotation (x=yaw, y=pitch) is a PlayerCamera
	// member rather than a function-local static so a second FPS-style camera
	// instance sharing this function (e.g. a spectator/cutscene camera) would
	// track its own look state instead of corrupting this one's — only one
	// camera calls this today, so that was latent rather than observed.
	//
	// This used to also write lookRotation.x/.y from the member `sensitivity`
	// a few lines above, but nothing ever read that write back — the actual
	// yaw/pitch below were driven by separate function-static locals and a
	// shadowing local `sensitivity` constant. `sensitivity` is a still-unwired
	// member meant for a future configurable look-speed setting; the constant
	// below preserves today's tuned feel until that's connected.
	constexpr float kLookSensitivity = -0.003f;

	Vector2 mouseDelta = GetMouseDelta();
	lookRotation.x += mouseDelta.x * kLookSensitivity;
	lookRotation.y += mouseDelta.y * kLookSensitivity;
	if (lookRotation.y > 1.5f) lookRotation.y = 1.5f;
	if (lookRotation.y < -1.5f) lookRotation.y = -1.5f;

	Vector3 camForward = {
		cosf(lookRotation.y) * sinf(lookRotation.x),
		sinf(lookRotation.y),
		cosf(lookRotation.y) * cosf(lookRotation.x)
	};

	// Camera Look Direction
	forward = camForward = Vector3Normalize(camForward);
	camera->target = Vector3Add(camera->position, Vector3Scale(camForward, 10.0f));

	// Update Player Direction to Match Camera Direction
	Vector3 flatForward = camForward;
	flatForward.y = 0;
	flatForward = Vector3Normalize(flatForward);

	// Update Camera Angles
	back = Vector3Scale(camForward, -1);
	right = Vector3{ -camForward.z, 0, camForward.x };
	left = Vector3{ camForward.z, 0, -camForward.x };
	up = Vector3{ 0, 1, 0 };
	down = Vector3{ 0, -1, 0 };

	// Update Player Angles
	if (Vector3Length(flatForward) > 0.001f) {
		player->rigidBody3D.forward = flatForward;
		player->rigidBody3D.back = Vector3Scale(flatForward, -1);
		player->rigidBody3D.right = Vector3{ -flatForward.z, 0, flatForward.x };
		player->rigidBody3D.left = Vector3{ flatForward.z, 0, -flatForward.x };
		player->rigidBody3D.up = Vector3{ 0, 1, 0 };
		player->rigidBody3D.down = Vector3{ 0, -1, 0 };
	}
	position = Vector3Add(player->rigidBody3D.translation, offset);
	camera->position = Vector3Add(player->rigidBody3D.translation, offset);
}