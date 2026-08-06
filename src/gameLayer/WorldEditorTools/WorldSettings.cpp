#include "WorldEditor.h"

#include <SaveSystem.h>

void NewWorld(Scene& scene) {

	scene.gameMap.create(Vector3(10.0f, 1.0f, 10.0f));
	scene.entities.clear();
	scene.interactables.clear();
	scene.ResetID();
}

void WorldEditor::ShowWorldSettings()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show World Settings Window
	ImGui::Begin("World Settings");

	/// Show Scene Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Scene Data");
	ImGui::Checkbox("Is 2D Active", &scene->is2DActive);
	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);
	ImGui::Checkbox("Limit Y Bounds", &scene->limitYBounds);
	ImGui::Separator();

	/// Show Game Map Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Game Map Data");

	ImGui::InputFloat3("Map Size", &scene->gameMap.size.x);
	ImGui::Text("Game Objects: %d", static_cast<int>(scene->gameMap.gameObjects.size()));
	ImGui::Text("Entities: %d", static_cast<int>(scene->entities.size()));
	ImGui::Text("Interactables: %d", static_cast<int>(scene->interactables.size()));
	ImGui::Text("Next ID: %llu", static_cast<unsigned long long>(scene->instanceHolder.idCounter));
	ImGui::Separator();

	if (ImGui::Button("Save Game")) { 
		SaveSystem::SaveGame("Default Save", scene); 
	}
	if (ImGui::Button("Load Game")) { 
		if (SaveSystem::LoadGame("Default Save", *scene)) {
			// IDs in the loaded scene may have changed, so reset the selection state to avoid dangling references
			ResetSelectionState();
			statusMessage = "Loaded Default Save";
		}
		else {
			statusMessage = "Failed to load Default Save";
		}
	}
	ImGui::Separator();

	/// World Save Data
	ImGui::BeginChild("World Saves");
	{
		ImGui::TextColored(ImVec4(255, 0, 255, 255), "World Data");

		char nameBuffer[128] = {};
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", worldName.c_str());
		if (ImGui::InputText("World Name", nameBuffer, sizeof(nameBuffer))) { worldName = nameBuffer; }

		if (ImGui::Button("New World")) { NewWorld(*scene); }
		if (ImGui::Button("Save World"))
		{
		statusMessage = SaveSystem::SaveWorld(worldName, scene) ? "Saved " + worldName + ".json" : "Failed to save " + worldName;
		}
		ImGui::SameLine();
		if (ImGui::Button("Load World"))
		{
			if (SaveSystem::LoadWorld(worldName, *scene))
			{
				// Resets World and Gizmos States
				ResetSelectionState();
				statusMessage = "Loaded " + worldName + ".json";
			}
			else
			{
				statusMessage = "Failed to load " + worldName;
			}
		}

		for (auto& file : SaveSystem::GetSaveFiles()) {
			std::string save = "Save " + file;
			std::string load = "Load " + file;
			ImGui::PushID(file.length());
			if (ImGui::Button(save.c_str())) {
				if (SaveSystem::SaveWorld(file, scene)) {
					ResetSelectionState();
					statusMessage = "Saved " + file;
				}
				else {
					statusMessage = "Failed To Save " + file;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(load.c_str())) {
				if (SaveSystem::LoadWorld(file, *scene)) {
					ResetSelectionState();
					statusMessage = "Loaded " + file;
				}
				else {
					statusMessage = "Failed To Load " + file;
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }

	ImGui::End();
}
