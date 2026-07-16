#include <Player.h>

#include <SceneManager.h>
///  Camera Update Function for First-Person Style Camera. 
///  Updates the camera's position to be at the player's head, and rotates based on mouse movement.
///  Also updates the player's direction vectors to match the camera's look direction.

void PlayerCamera::UpdateCameraFPS(Camera* camera)
{
	auto player = SceneManager::getInstance().currentScene->player;

	auto up = Vector3(0.0f, 1.0f, 0.0f);
	offset = Vector3(0, player->rigidBody3D.scale.y / 2, 0); // Camera offset to be at player's head

	if (InputSystem::getInstance().IsActionDown(ACTION_MOVE_CROUCH)) { offset.y /= 2; }

	position = Vector3Add(player->rigidBody3D.translation, offset);
	lookRotation.x -= GetMouseDelta().x * sensitivity.x;
	lookRotation.y += GetMouseDelta().y * sensitivity.y;


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
	Vector3 flatForward = camForward;
	flatForward.y = 0;
	flatForward = Vector3Normalize(flatForward);

	player->camera.back = Vector3Scale(flatForward, -1);
	player->camera.right = Vector3{ -flatForward.z, 0, flatForward.x };
	player->camera.left = Vector3{ flatForward.z, 0, -flatForward.x };
	player->camera.up = Vector3{ 0, 1, 0 };
	player->camera.down = Vector3{ 0, -1, 0 };


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