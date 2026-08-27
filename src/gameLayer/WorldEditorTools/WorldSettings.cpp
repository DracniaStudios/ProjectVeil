#include "WorldEditor.h"

#include <SaveSystem.h>

void NewWorld(Scene& scene) {

	// ~Entity()/~InteractableObject() never free GPU resources (see
	// GameMap::removeObject()); release each object's generated fallback model
	// before the clears below erase it, or it leaks.
	for (auto& [id, entity] : scene.entities) { entity->releaseGeneratedModel(); }
	for (auto& [id, interactable] : scene.interactables) { interactable->releaseGeneratedModel(); }

	scene.gameMap.create(Vector3(10.0f, 1.0f, 10.0f));
	scene.entities.clear();
	scene.interactables.clear();

	// Player::inventory holds non-owning references into the interactables map
	// just cleared above (see GameMap::removeInteractable for the same hazard)
	// — left alone here it dangles the moment the Player Inspector reads it.
	// Player::interactObjectId is an id into the same map and goes stale the
	// same way, so it is reset alongside it.
	if (scene.player) {
		scene.player->inventory.clear();
		scene.player->interactObjectId = 0;
	}

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

	// Spawn point. Authored here and saved with the world; LoadWorld places the
	// player on it. A world with none leaves the player at the RigidBody3D
	// default of (0,0,0), which is how chunk_1 used to start every session
	// inside the artifact display plinth.
	if (ImGui::InputFloat3("Spawn Point", &scene->gameMap.spawnPoint.x))
	{
		scene->gameMap.hasSpawnPoint = true;
	}
	if (ImGui::Button("Set Spawn To Player") && scene->player)
	{
		scene->gameMap.spawnPoint = scene->player->getPosition();
		scene->gameMap.hasSpawnPoint = true;
		statusMessage = "Spawn point set to player position";
	}
	if (!scene->gameMap.hasSpawnPoint)
	{
		ImGui::TextColored(ImVec4(255, 255, 0, 255),
			"No spawn point: player starts wherever it already was.");
	}
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
			ImGui::PushID(file.length());
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
