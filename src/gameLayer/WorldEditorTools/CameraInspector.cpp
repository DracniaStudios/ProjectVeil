#include "WorldEditor.h"

void WorldEditor::ShowCameraData(Player* player)
{
	const auto camera = &SceneManager::getInstance().camera3D;
	/// Show Camera Data Window
	ImGui::Begin("Camera Data");

	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", camera->position.x, camera->position.y, camera->position.z);
	ImGui::Text("Camera Target: (%.2f, %.2f, %.2f)", camera->target.x, camera->target.y, camera->target.z);
	ImGui::Text("Camera Up: (%.2f, %.2f, %.2f)", camera->up.x, camera->up.y, camera->up.z);
	ImGui::Text("Camera FOVY: (%.2f)", camera->fovy);
	ImGui::Text("Camera Projection: %d", camera->projection);
	ImGui::Separator();

	ImGui::Text("Mouse Position: (%.2f, %.2f)", GetMousePosition().x, GetMousePosition().y);
	ImGui::Text("Mouse Delta: (%.2f, %.2f)", GetMouseDelta().x, GetMouseDelta().y);
	ImGui::Separator();

	ImGui::End();
}
