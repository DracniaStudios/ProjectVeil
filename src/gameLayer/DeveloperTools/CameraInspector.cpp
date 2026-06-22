#include "DeveloperWindow.h"

void DeveloperWindow::ShowCameraData(Player* player)
{
	auto camera = *SceneManager::getInstance().currentScene->camera;
	/// Show Camera Data Window
	ImGui::Begin("Camera Data");

	ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", camera.forward.x, camera.forward.y, camera.forward.z);
	ImGui::Text("Camera Offset: (%.2f, %.2f, %.2f)", camera.offset.x, camera.offset.y, camera.offset.z);
	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", camera.position.x, camera.position.y, camera.position.z);
	ImGui::Text("Camera Sensitivity: (%.2f, %.2f)", camera.sensitivity.x, camera.sensitivity.y);
	ImGui::Text("Camera Look Rotation: (%.2f, %.2f)", camera.lookRotation.x, camera.lookRotation.y);
	ImGui::Text("Camera Lean: (%.2f, %.2f)", camera.lean.x, camera.lean.y);
	ImGui::Separator();

	ImGui::Text("Mouse Position: (%.2f, %.2f)", GetMousePosition().x, GetMousePosition().y);
	ImGui::Text("Mouse Delta: (%.2f, %.2f)", GetMouseDelta().x, GetMouseDelta().y);
	ImGui::Separator();

	ImGui::End();
}
