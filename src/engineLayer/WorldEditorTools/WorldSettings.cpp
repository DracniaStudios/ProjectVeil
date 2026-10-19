#include "WorldEditor.h"

#include <SaveSystem.h>

namespace Editor {

static void NewWorld(SceneManagement::Scene& scene) {

	// DestroyEntity/DestroyInteractable each already release a generated
	// fallback model before erasing (see GameMap::DestroyGameObject()) — snapshot
	// ids first since destroying while iterating the same map is unsafe.
	std::vector<uint64_t> entityIds;
	scene.gameMap.ForEachEntity([&](Entity& entity) { entityIds.push_back(entity.id); return true; });
	for (auto id : entityIds) { scene.gameMap.DestroyEntity(id); }

	std::vector<uint64_t> interactableIds;
	scene.gameMap.ForEachInteractable([&](InteractableObject& interactable) { interactableIds.push_back(interactable.id); return true; });
	for (auto id : interactableIds) { scene.gameMap.DestroyInteractable(id); }

	scene.gameMap.create(Vector3(10.0f, 1.0f, 10.0f));

	// Player::inventory holds non-owning references into the interactables map
	// just cleared above (see GameMap::DestroyInteractable for the same hazard) —
	// left alone they dangle the moment the Player Inspector reads the inventory.
	if (scene.player) {
		scene.player->inventory.clear();
	}

	scene.ResetID();
}

void WorldEditor::ShowWorldSettings()
{
	auto scene = SceneManagement::SceneManager::getInstance().currentScene;

	/// Show World Settings Window
	ImGui::Begin("World Settings");
	ImGui::TextDisabled("%s", std::to_string(GetFrameTime()).c_str());

	/// Show Scene Data
	ImGui::TextColored(ImVec4(1, 0, 1, 1), "Scene Data");

	// Read-only. Overlays open and close through their own rules, never by hand
	// here, so the editor can't leave them in a state the game itself can't reach.
	std::string openOverlays;
	for (Overlay overlay : scene->overlays.OpenOverlays()) {
		if (!openOverlays.empty()) { openOverlays += " > "; }
		openOverlays += OverlayName(overlay);
	}
	ImGui::Text("Overlays: %s", openOverlays.empty() ? "none (gameplay has input)" : openOverlays.c_str());
	if (ImGui::Button("Close All Overlays")) {
		// A MiniGame overlay is only closed alongside ending its game; closing it
		// alone would leave the game waiting with nothing on screen.
		if (scene->miniGameSession.IsRunning()) { scene->QuitMiniGame(); }
		scene->overlays.CloseAll();
	}

	ImGui::Checkbox("Limit Y Bounds", &scene->limitYBounds);
	ImGui::Separator();

	/// Show Game Map Data
	ImGui::TextColored(ImVec4(1, 0, 1, 1), "Game Map Data");

	ImGui::InputFloat3("Map Size", &scene->gameMap.size.x);


	ImGui::Text("Game Objects: %d", static_cast<int>(scene->gameMap.GameObjectCount()));
	ImGui::Text("Entities: %d", static_cast<int>(scene->gameMap.EntityCount()));
	ImGui::Text("Interactables: %d", static_cast<int>(scene->gameMap.InteractableCount()));
	ImGui::Text("Next ID: %llu", static_cast<unsigned long long>(scene->gameMap.instanceHolder.idCounter));
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
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "World Data");

		char nameBuffer[128] = {};
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", worldName.c_str());
		if (ImGui::InputText("World Name", nameBuffer, sizeof(nameBuffer))) { worldName = nameBuffer; }

		if (ImGui::Button("New World")) {
			NewWorld(*scene);
			// NewWorld() renumbers every remaining object via ResetID(), exactly
			// like Load Game/Load World — stale selection/undo state would
			// otherwise reference ids that now belong to different objects.
			ResetSelectionState();
		}
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
			ImGui::PushID(file.c_str());
			if (ImGui::Button(save.c_str())) {
				if (SaveSystem::SaveWorld(file, scene)) {
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
}
