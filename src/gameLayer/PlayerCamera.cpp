#include "PlayerCamera.h"

#include <Player.h>

void PlayerCamera::UpdateCameraFPS(Player* player)
{
    char sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
    char forward = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));

    Vector2 mouseDelta = GetMouseDelta();
	lookRotation.x -= mouseDelta.x * sensitivity.x;
	lookRotation.y -= mouseDelta.y * sensitivity.y;

    float delta = GetFrameTime();
    headLerp = Lerp(headLerp, (player->isCrouching ? 1.0f : 0.0f), delta * 20.0f);

	camera3D.position = Vector3(
		player->getPosition().x,
		player->getPosition().y + (player->rigidBody3D.scale.y / 2) + headLerp,
		player->getPosition().z
	);

	if (player->rigidBody3D.downTouch && Vector2Length(mouseDelta) > 0.0f)
	{
		headTimer += delta * 10.0f;
        walkLerp = Lerp(walkLerp, 1.0f, 10.0 * delta);
		camera3D.fovy = Lerp(camera3D.fovy, 90.0f, 10.0f * delta);
	}
	else
	{
		headTimer = 0.0f;
        walkLerp = Lerp(walkLerp, 0.0f, 10.0f * delta);
        camera3D.fovy = Lerp(camera3D.fovy, 60.0f, 5.0f * delta);
	}

    lean.x = Lerp(lean.x, sideway * 0.02f, 10.0f * delta);
    lean.y = Lerp(lean.y, forward * 0.015f, 10.0f * delta);

    const Vector3 up = Vector3{ 0.0f, 1.0f, 0.0f };
    const Vector3 targetOffset = Vector3{ 0.0f, 0.0f, -1.0f };

    // Left and right
    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, lookRotation.x);

    // Clamp view up
    float maxAngleUp = Vector3Angle(up, yaw);
    maxAngleUp -= 0.001f; // Avoid numerical errors
    if (-(lookRotation.y) > maxAngleUp) { lookRotation.y = -maxAngleUp; }

    // Clamp view down
    float maxAngleDown = Vector3Angle(Vector3Negate(up), yaw);
    maxAngleDown *= -1.0f; // Downwards angle is negative
    maxAngleDown += 0.001f; // Avoid numerical errors
    if (-(lookRotation.y) < maxAngleDown) { lookRotation.y = -maxAngleDown; }

    // Up and down
    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    // Rotate view vector around right axis
    float pitchAngle = -lookRotation.y - lean.y;
    pitchAngle = Clamp(pitchAngle, -PI / 2 + 0.0001f, PI / 2 - 0.0001f); // Clamp angle so it doesn't go past straight up or straight down
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    // Head animation
    // Rotate up direction around forward axis
    float headSin = sinf(headTimer * PI);
    float headCos = cosf(headTimer * PI);
    const float stepRotation = 0.01f;
    camera3D.up = Vector3RotateByAxisAngle(up, pitch, headSin * stepRotation + lean.x);

    // Camera BOB
    const float bobSide = 0.1f;
    const float bobUp = 0.15f;
    Vector3 bobbing = Vector3Scale(right, headSin * bobSide);
    bobbing.y = fabsf(headCos * bobUp);

    camera3D.position = Vector3Add(camera3D.position, Vector3Scale(bobbing, walkLerp));
    camera3D.target = Vector3Add(camera3D.position, pitch);
}