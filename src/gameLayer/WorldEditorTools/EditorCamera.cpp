#include <WorldEditor.h>

void SelectObjectInWorldSpace(EditorCamera* editor, Camera3D* camera) {
	auto gameObjects = SceneManager::getInstance().currentScene->gameMap.gameObjects;
	// Get Mouse To World Space.
	const auto mouseRay = GetScreenToWorldRay(GetMousePosition(), *camera);

	// Dangerous
	for (size_t i = 0; i < gameObjects.size(); ++i) {
		auto object = &gameObjects[i];
		if (!object->isSelectable) continue;
		// Check Mouse Ray and Object Collision
		if (auto collider = GetRayCollisionBox(mouseRay, object->rigidBody3D.collisionBox); collider.hit) {
			auto worldEditor = &WorldEditor::getInstance();

			worldEditor->SelectObject(object->id);
			object->displayCollider = true;
			object->displayDirection = true;

		}
	}
}

void UpdateDirection(EditorCamera* editor, Camera3D* camera) {

	auto up = Vector3(0.0f, 1.0f, 0.0f);

	// FPS-style camera look
	static float yaw = 0.0f;
	static float pitch = 0.0f;
	const float sensitivity = -0.1f;

	Vector2 mouseDelta = GetMouseDelta();
	yaw += mouseDelta.x * sensitivity * GetFrameTime();
	pitch += mouseDelta.y * sensitivity * GetFrameTime();
	if (pitch > 1.5f) pitch = 1.5f;
	if (pitch < -1.5f) pitch = -1.5f;

	editor->lookRotation.x -= mouseDelta.x + sensitivity * GetFrameTime();
	editor->lookRotation.x += mouseDelta.x + sensitivity * GetFrameTime();

	Vector3 camForward = {
		cosf(pitch) * sinf(yaw),
		sinf(pitch),
		cosf(pitch) * cosf(yaw)
	};

	// Camera Look Direction
	editor->forward = camForward = Vector3Normalize(camForward);
	camera->target = Vector3Add(camera->position, Vector3Scale(camForward, 10.0f));

	// Set Camera Direction

	// Update EditorCamera Angles
	editor->back = Vector3Scale(camForward, -1);
	editor->right = Vector3{ -camForward.z, 0, camForward.x };
	editor->left = Vector3{ camForward.z, 0, -camForward.x };
	camera->up = editor->up = Vector3{ 0, 1, 0 };
	editor->down = Vector3{ 0, -1, 0 };

	UpdateCamera(camera, CAMERA_CUSTOM);
}

void UpdateMovement(EditorCamera* editor, Camera3D* camera) {
	auto speed = 5;
	auto inputSystem = &InputSystem::getInstance();

	if (inputSystem->IsActionDown(ACTION_MOVE_SPRINT)) { speed *= 2; }

	if (inputSystem->IsActionDown(ACTION_EDITOR_UP)) { editor->position.y += speed * 0.1f; }
	if (inputSystem->IsActionDown(ACTION_EDITOR_DOWN)) { editor->position.y -= speed * 0.1f; }

	Vector2 moveDirection = Vector2Zero();

	moveDirection.x = inputSystem->IsActionDown(ACTION_MOVE_LEFT) ? -1.0 :
		inputSystem->IsActionDown(ACTION_MOVE_RIGHT) ? 1.0f : 0;
	moveDirection.y = inputSystem->IsActionDown(ACTION_MOVE_FORWARD) ? 1.0 :
		inputSystem->IsActionDown(ACTION_MOVE_BACKWARD) ? -1.0f : 0;

	if (moveDirection.y > 0) { editor->position += (editor->forward * speed) * 0.1f; }
	if (moveDirection.y < 0) { editor->position += (editor->back * speed) * 0.1f; }
	if (moveDirection.x < 0) { editor->position += (editor->left * speed) * 0.1f; }
	if (moveDirection.x > 0) { editor->position += (editor->right * speed) * 0.1f; }

	camera->position = editor->position;
}

void EditorCamera::Update(Camera3D* camera) {
	
	UpdateDirection(this, camera);
	
	UpdateMovement(this, camera);

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { SelectObjectInWorldSpace(this, camera); }

};