#include "PlayerCamera.h"

#include <Player.h>

void PlayerCamera::UpdateCameraFPS(Camera* camera, Player* player)
{

	camera->position = Vector3(
		player->getPosition().x,
		player->getPosition().y + (player->rigidBody3D.scale.y / 2),
		player->getPosition().z
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
	camForward = Vector3Normalize(camForward);
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