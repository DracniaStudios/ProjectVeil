#include "WorldEditor.h"

#include <SaveSystem.h>

void WorldEditor::ShowWorldSettings()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show World Settings Window
	ImGui::Begin("World Settings");

	/// Show Scene Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Scene Data");
	if (ImGui::ArrowButton("Reset IDs", ImGuiDir_Right)) { scene->ResetID(); }
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

	/// World Save Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "World Data");

	if (ImGui::Button("Save World"))
	{
		statusMessage = SaveSystem::SaveWorld(scene) ? "Saved world.json" : "Failed to save world";
	}
	if (ImGui::Button("Load World"))
	{
		if (SaveSystem::LoadWorld(*scene))
		{
			selectedObjectId = 0; // Ids from the old world are stale
			statusMessage = "Loaded world.json";
		}
		else
		{
			statusMessage = "Failed to load world";
		}
	}
	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }

	ImGui::End();
}
