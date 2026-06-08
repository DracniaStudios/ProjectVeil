#include "DeveloperWindow.h"

void DeveloperWindow::ShowGameData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;
	ImGui::Begin("Game Data");

	/// Show Scene Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Scene Data");

	ImGui::Checkbox("Is 2D Active", &scene->is2DActive);
	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);
	ImGui::Separator();

	/// Show Game Map Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Game Map Data");

	ImGui::Text("Game Objects: %d", static_cast<int>(scene->gameMap.gameObjects.size()));
	ImGui::Text("Game Map Size: (%.2f, %.2f, %.2f)", &scene->gameMap.size.x);

	/// Entity Data
	ImGui::Text("Entity Count: %f", scene->entities.size());
	ImGui::Text("Last ID Used: %f", &scene->instanceHolder.idCounter);

	ImGui::End();
}