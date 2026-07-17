#include <WorldEditor.h>

void WorldEditor::ShowHierarchy() {

	ImGui::Begin("Hierarchy");

	for (auto& object : SceneManager::getInstance().currentScene->gameMap.gameObjects) {
		ImGui::PushID(&object);
		// Set Inspector Object To this object
		if (ImGui::Button(object.name.c_str())) { inspectObjectId = object.id; }
		ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::Button("Enabled", {60, 0})) { object.isEnabled = !object.isEnabled; }
		ImGui::PopID();
	}

	ImGui::End();
}