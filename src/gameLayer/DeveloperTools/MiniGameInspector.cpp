#include <DeveloperWindow.h>

void DeveloperWindow::ShowMiniGameData(Player* player)
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

		ImGui::Text("Obstacles: %d", scene->miniGame->data->obstacles.size());
		for (auto& obj : scene->miniGame->data->obstacles)
		{
			ImGui::PushID(&obj);
			if (ImGui::Button(std::to_string(obj.x).c_str()))
			{
				miniGameObject = &obj;
			}
			ImGui::PopID();
		}
		ImGui::Separator();

		if (miniGameObject != nullptr)
		{
			auto obj = static_cast<Rectangle*>(miniGameObject);
			ImGui::Text("Object: X %f", obj->x);
			ImGui::Text("Object: Y %f", obj->y);
			ImGui::Text("Object: Width %f", obj->width);
			ImGui::Text("Object: Height %f", obj->height);
		}
		ImGui::Separator();

	}

	ImGui::End();
}