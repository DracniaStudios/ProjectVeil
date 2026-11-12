#include <Player.h>

#include <raymath.h>

#include <cmath>

/// First-person look. The simulation keeps the player's look as two angles and
/// derives the direction vectors from them every tick (SetLook); the client
/// places the camera it draws with from the same angles (PlaceCamera), so the
/// two never disagree about where "forward" is.

Vector3 PlayerCamera::LookDirection(float yaw, float pitch)
{
	const Vector3 direction = {
		cosf(pitch) * sinf(yaw),
		sinf(pitch),
		cosf(pitch) * cosf(yaw)
	};
	return Vector3Normalize(direction);
}

void PlayerCamera::SetLook(float yaw, float pitch, Physics3D::RigidBody3D& body)
{
	const Vector3 camForward = LookDirection(yaw, pitch);

	// Camera Look Direction
	forward = camForward;
	back = Vector3Scale(camForward, -1);
	right = Vector3{ -camForward.z, 0, camForward.x };
	left = Vector3{ camForward.z, 0, -camForward.x };
	up = Vector3{ 0, 1, 0 };
	down = Vector3{ 0, -1, 0 };

	// Update Player Direction to Match Camera Direction
	Vector3 flatForward = camForward;
	flatForward.y = 0;
	if (Vector3Length(flatForward) > 0.001f) {
		flatForward = Vector3Normalize(flatForward);
		body.forward = flatForward;
		body.back = Vector3Scale(flatForward, -1);
		body.right = Vector3{ -flatForward.z, 0, flatForward.x };
		body.left = Vector3{ flatForward.z, 0, -flatForward.x };
		body.up = Vector3{ 0, 1, 0 };
		body.down = Vector3{ 0, -1, 0 };
	}
}

void PlayerCamera::PlaceCamera(Camera3D* camera, Vector3 eye, float yaw, float pitch)
{
	camera->position = eye;
	camera->target = Vector3Add(eye, Vector3Scale(LookDirection(yaw, pitch), 10.0f));
	camera->up = Vector3{ 0, 1, 0 };
}
