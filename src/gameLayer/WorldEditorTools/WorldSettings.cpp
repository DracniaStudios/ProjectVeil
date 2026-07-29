#include "WorldEditor.h"

#include <SaveSystem.h>

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

	if (ImGui::Button("Save Game")) { SaveSystem::SaveGame("Default Save", scene); }
	if (ImGui::Button("Load Game")) { SaveSystem::LoadGame("Default Save", *scene); }
	ImGui::Separator();

	/// World Save Data
	ImGui::BeginChild("World Saves");
	{
		ImGui::TextColored(ImVec4(255, 0, 255, 255), "World Data");

		ImGui::RadioButton("Save Game", &saveState, 0);
		ImGui::RadioButton("Load Game", &saveState, 1);

		if (ImGui::Button("Default Save World"))
		{
		statusMessage = SaveSystem::SaveWorld("world", scene) ? "Saved world.json" : "Failed to save world";
		}
		ImGui::SameLine();
		if (ImGui::Button("Default Load World"))
		{
			if (SaveSystem::LoadWorld("world", *scene))
			{
				selectedObjectId = 0; // Ids from the old world are stale
				statusMessage = "Loaded world.json";
			}
			else
			{
				statusMessage = "Failed to load world";
			}
		}

		for (auto& file : SaveSystem::GetSaveFiles()) {

			std::string save = "Save " + file;
			std::string load = "Load " + file;
			if (ImGui::Button(save.c_str())) {
				if (SaveSystem::SaveWorld(file, scene)) {
					selectedObjectId = 0;
					statusMessage = "Saved " + file;
				}
				else {
					statusMessage = "Failed To Save " + file;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(load.c_str())) {
				if (SaveSystem::LoadWorld(file, *scene)) {
					selectedObjectId = 0;
					statusMessage = "Loaded " + file;
				}
				else {
					statusMessage = "Failed To Load " + file;
				}
			}
		}
	}
	ImGui::EndChild();
	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }

	ImGui::End();
}
