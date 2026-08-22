#include <WorldEditor.h>

#include <cmath>

namespace
{
	// Look angles are clamped just short of straight up/down. At exactly +/-pi/2
	// the forward vector becomes parallel to the world up axis and the right/left
	// basis derived from it collapses to zero, which makes strafing stop working
	// and the view roll unpredictably.
	constexpr float kMaximumPitch = 1.5f;

	constexpr float kLookSensitivity = -0.005f;
	constexpr float kBaseSpeed = 5.0f;
	constexpr float kSprintMultiplier = 2.0f;

	// The original movement code advanced a fixed distance per frame. Scaling by
	// deltaTime against 60 FPS keeps the tuned feel while making the camera
	// frame-rate independent; the cap stops a single long frame (an asset load,
	// a shader recompile) from launching the camera across the map.
	float FrameStep()
	{
		return Clamp(GetFrameTime() * 60.0f, 0.0f, 3.0f);
	}

	/** Rebuilds the camera basis and target from the current yaw/pitch. */
	void ApplyOrientation(EditorCamera* editor, Camera3D* camera)
	{
		const Vector3 forward = Vector3Normalize(Vector3{
			cosf(editor->pitch) * sinf(editor->yaw),
			sinf(editor->pitch),
			cosf(editor->pitch) * cosf(editor->yaw)
		});

		editor->forward = forward;
		editor->back = Vector3Scale(forward, -1.0f);
		editor->right = Vector3{ -forward.z, 0.0f, forward.x };
		editor->left = Vector3{ forward.z, 0.0f, -forward.x };
		editor->up = Vector3{ 0.0f, 1.0f, 0.0f };
		editor->down = Vector3{ 0.0f, -1.0f, 0.0f };

		camera->up = editor->up;

		// The target has to be rebuilt every frame, not only while looking
		// around. It used to be written exclusively inside the look handler, so
		// flying with WASD moved the position while the target stayed pinned to
		// wherever the mouse last left it — the view swung around a fixed point
		// instead of moving forward.
		camera->position = editor->position;
		camera->target = Vector3Add(editor->position, Vector3Scale(forward, 10.0f));
	}

	void UpdateLook(EditorCamera* editor)
	{
		const Vector2 mouseDelta = GetMouseDelta();

		editor->yaw += mouseDelta.x * kLookSensitivity;
		editor->pitch += mouseDelta.y * kLookSensitivity;
		editor->pitch = Clamp(editor->pitch, -kMaximumPitch, kMaximumPitch);
	}

	void UpdateMovement(EditorCamera* editor)
	{
		auto inputSystem = &InputSystem::getInstance();

		float speed = kBaseSpeed * 0.1f * FrameStep();
		if (inputSystem->IsActionDown(ACTION_MOVE_SPRINT)) { speed *= kSprintMultiplier; }

		if (inputSystem->IsActionDown(ACTION_EDITOR_UP)) { editor->position.y += speed; }
		if (inputSystem->IsActionDown(ACTION_EDITOR_DOWN)) { editor->position.y -= speed; }

		if (inputSystem->IsActionDown(ACTION_MOVE_FORWARD)) { editor->position += editor->forward * speed; }
		if (inputSystem->IsActionDown(ACTION_MOVE_BACKWARD)) { editor->position += editor->back * speed; }
		if (inputSystem->IsActionDown(ACTION_MOVE_LEFT)) { editor->position += editor->left * speed; }
		if (inputSystem->IsActionDown(ACTION_MOVE_RIGHT)) { editor->position += editor->right * speed; }
	}
}

void EditorCamera::SyncFrom(const Camera3D& camera)
{
	position = camera.position;

	// Recover yaw/pitch from the pose being adopted, using the inverse of the
	// forward vector built in ApplyOrientation. Starting from zeroed angles
	// instead would snap the view to a fixed heading the moment the editor opens.
	const Vector3 forward = Vector3Subtract(camera.target, camera.position);
	if (Vector3LengthSqr(forward) > 1.0e-6f)
	{
		const Vector3 direction = Vector3Normalize(forward);
		pitch = Clamp(asinf(Clamp(direction.y, -1.0f, 1.0f)), -kMaximumPitch, kMaximumPitch);
		yaw = atan2f(direction.x, direction.z);
	}

	isSeeded = true;
}

void EditorCamera::FocusOn(Vector3 target, float distance)
{
	// Pull back along the current heading rather than choosing a new one, so
	// focusing reframes the object without also disorienting the user.
	if (Vector3LengthSqr(forward) < 1.0e-6f) { forward = Vector3{ 0.0f, 0.0f, 1.0f }; }
	position = Vector3Subtract(target, Vector3Scale(Vector3Normalize(forward), fmaxf(distance, 1.0f)));
}

void EditorCamera::Update(Camera3D* camera)
{
	if (camera == nullptr) { return; }

	// First activation adopts the camera being replaced. WorldEditor::update
	// normally does this on the F1 edge; this covers any path that reaches the
	// editor camera without one (a scene constructed with the editor already on).
	if (!isSeeded) { SyncFrom(*camera); }

	const ImGuiIO& io = ImGui::GetIO();

	// Dragging inside an ImGui window must not also spin the camera, and typing
	// into a text field must not fly it: WASD are movement bindings, so naming
	// an object would otherwise send the viewport across the map.
	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !io.WantCaptureMouse) { UpdateLook(this); }
	if (!io.WantCaptureKeyboard) { UpdateMovement(this); }

	// Orientation is applied unconditionally so the target tracks the position
	// even on frames where neither look nor movement ran.
	ApplyOrientation(this, camera);

	// Object picking deliberately does not live here anymore. It moved to
	// WorldEditor::UpdateViewportInput, where it can be ordered against the
	// gizmo and the placement tool — clicking a gizmo handle used to also
	// reselect whatever object happened to be behind it.
}
