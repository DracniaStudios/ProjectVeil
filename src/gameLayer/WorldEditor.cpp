#include <WorldEditor.h>
#include <DeveloperWindow.h>

void WorldEditor::Enable()
{
	isEnabled = true;
	isActive = true;

	activeScene = SceneManager::getInstance().currentScene;
	SceneManager::getInstance().camera3D = editorCamera;

	editorCamera.position = Vector3(0, 10, 10);
	editorCamera.target = Vector3(0, 0, 0);
	editorCamera.up = Vector3(0, 1, 0);
	editorCamera.fovy = 45.0f;

}

void WorldEditor::Disable()
{
	isEnabled = false;
	isActive = false;
	SceneManager::getInstance().camera3D = {};
}

void WorldEditor::Update(float deltaTime)
{
	if (!isEnabled) { return; }
	// Update Editor Camera
	UpdateCamera(&editorCamera, CAMERA_FIRST_PERSON);
	// Toggle Grid
	if (IsKeyPressed(KEY_G)) { isGridActive = !isGridActive; }
	// Toggle Snap
	if (IsKeyPressed(KEY_S)) { isSnapActive = !isSnapActive; }
}

void WorldEditor::Draw2D()
{
	if (!isEnabled) { return; }
	// Draw 2D Editor UI
}

void WorldEditor::Draw3D()
{
	if (!isEnabled) { return; }
	// Draw 3D Editor Grid
	if (isGridActive) { DrawGrid3D(100.0f, gridSize); }
}

void WorldEditor::DrawGrid(float size, float spacing)
{
	for (float x = -size; x <= size; x += spacing)
	{
		DrawLine3D(Vector3(x, 0, -size), Vector3(x, 0, size), LIGHTGRAY);
	}
	for (float z = -size; z <= size; z += spacing)
	{
		DrawLine3D(Vector3(-size, 0, z), Vector3(size, 0, z), LIGHTGRAY);
	}
}

void WorldEditor::DrawGrid3D(float size, float spacing)
{
	for (float x = -size; x <= size; x += spacing)
	{
		DrawLine3D(Vector3(x, 0, -size), Vector3(x, 0, size), LIGHTGRAY);
	}
	for (float z = -size; z <= size; z += spacing)
	{
		DrawLine3D(Vector3(-size, 0, z), Vector3(size, 0, z), LIGHTGRAY);
	}
}