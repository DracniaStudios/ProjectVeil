#include <WorldEditor.h>

void WorldEditor::ShowMiniGameData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Begin Mini Game Data Window
	ImGui::Begin("Mini Game Data");

	ImGui::Text("Display Mini Game Data");
	ImGui::InputInt("Mini Game ID", &currentGameID, 1, 1);
	currentGameID = Clamp(currentGameID, 0, 6);

	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);

	/// Launch Mini Game Button
	if (ImGui::Button("Launch Mini Game"))
	{
		miniGameObstacleIndex = -1; // Selection belongs to the previous game's obstacle list
		switch (currentGameID)
		{
			// Select Mini Game
			case 1: scene->miniGame = MiniGame_Crane(player);break;
			case 2: scene->miniGame = MiniGame_Doctor(player);break;
			case 3: scene->miniGame = MiniGame_SimonSays(player);break;
			case 4: scene->miniGame = MiniGame_Maze(player);break;
			case 5: scene->miniGame = MiniGame_RoShamBoo(player);break;
			default: scene->miniGame = MiniGame_FlappyBird(player); break;
		}
	}
	ImGui::Separator();

	/// Show Mini Game Data
	ImGui::TextColored(ImVec4(0, 0, 255, 255), "Mini Game Data");

	if (scene->miniGame != nullptr)
	{
		ImGui::Text("Score: %d", scene->miniGame->data->score);
		ImGui::Text("Score Goal: %d", scene->miniGame->data->scoreGoal);
		ImGui::InputFloat4("Screen Size: %d", &scene->miniGame->data->screen.x);
		ImGui::InputFloat4("Goal Size: %d", &scene->miniGame->data->goal.x);
		ImGui::Separator();

		auto& obstacles = scene->miniGame->data->obstacles;
		ImGui::Text("Obstacles: %d", static_cast<int>(obstacles.size()));
		for (int i = 0; i < static_cast<int>(obstacles.size()); i++)
		{
			ImGui::PushID(i);
			if (ImGui::Button(std::to_string(obstacles[i].x).c_str()))
			{
				miniGameObstacleIndex = i;
			}
			ImGui::PopID();
		}
		ImGui::Separator();

		// Index re-validated every frame — games clear and refill the obstacle list during play
		if (miniGameObstacleIndex >= 0 && miniGameObstacleIndex < static_cast<int>(obstacles.size()))
		{
			Rectangle& obj = obstacles[miniGameObstacleIndex];
			ImGui::Text("Object: X %f", obj.x);
			ImGui::Text("Object: Y %f", obj.y);
			ImGui::Text("Object: Width %f", obj.width);
			ImGui::Text("Object: Height %f", obj.height);
		}
		ImGui::Separator();

	}

	ImGui::End();
}