#include "DeveloperWindow.h"

#include <SaveSystem.h>

void DeveloperWindow::ShowGameData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;
	ImGui::Begin("Game Data");

	/// Show Scene Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Scene Data");

	ImGui::Checkbox("Is 2D Active", &scene->is2DActive);
	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);
	ImGui::Checkbox("Limit Y Bounds", &scene->limitYBounds);
	ImGui::Separator();

	/// Show Game Map Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Game Map Data");

	ImGui::Text("Game Objects: %d", static_cast<int>(scene->gameMap.gameObjects.size()));
	ImGui::Text("Game Map Size: (%.2f, %.2f, %.2f)", scene->gameMap.size.x, scene->gameMap.size.y, scene->gameMap.size.z);

	/// Entity Data
	ImGui::Text("Entity Count: %d", static_cast<int>(scene->entities.size()));
	ImGui::Text("Last ID Used: %llu", static_cast<unsigned long long>(scene->instanceHolder.idCounter));
	ImGui::Separator();


	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Save Data");

	if (ImGui::Button("Save Game")) { SaveSystem::SaveGame(RESOURCES_PATH "../saves/game.json", scene); }
	if (ImGui::Button("Load Game")) { SaveSystem::LoadGame(RESOURCES_PATH "../saves/game.json", *scene); }
	
	ImGui::End();
}