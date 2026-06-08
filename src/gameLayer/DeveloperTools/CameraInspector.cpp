#include "DeveloperWindow.h"

void DeveloperWindow::ShowCameraData(Player* player)
{
	/// Show Camera Data Window
	ImGui::Begin("Camera Data");

	ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", player->camera.forward.x, player->camera.forward.y, player->camera.forward.z);
	ImGui::Text("Camera Offset: (%.2f, %.2f, %.2f)", player->camera.offset.x, player->camera.offset.y, player->camera.offset.z);
	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", player->camera.position.x, player->camera.position.y, player->camera.position.z);
	ImGui::Text("Camera Sensitivity: (%.2f, %.2f)", player->camera.sensitivity.x, player->camera.sensitivity.y);
	ImGui::Text("Camera Look Rotation: (%.2f, %.2f)", player->camera.lookRotation.x, player->camera.lookRotation.y);
	ImGui::Text("Camera Lean: (%.2f, %.2f)", player->camera.lean.x, player->camera.lean.y);
	ImGui::Separator();

	ImGui::Text("Mouse Position: (%.2f, %.2f)", GetMousePosition().x, GetMousePosition().y);
	ImGui::Text("Mouse Delta: (%.2f, %.2f)", GetMouseDelta().x, GetMouseDelta().y);
	ImGui::Separator();

	ImGui::End();
}
