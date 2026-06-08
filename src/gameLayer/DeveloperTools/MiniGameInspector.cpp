#include <DeveloperWindow.h>

void DeveloperWindow::ShowMiniGameData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Begin Mini Game Data Window
	ImGui::Begin("Mini Game Data");

	ImGui::Text("Display Mini Game Data");
	ImGui::InputInt("Mini Game ID", &currentGameID, 1, 1);
	currentGameID = Clamp(currentGameID, 0, 2);

	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);

	/// Launch Mini Game Button
	if (ImGui::Button("Launch Mini Game"))
	{
		switch (currentGameID)
		{
			// Select Mini Game
		case 1:
			scene->miniGame = MiniGame_crane();
			scene->isMiniActive = true;
			player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.2f));
			break;
		default:
			scene->miniGame = MiniGame_flappyBird();
			scene->isMiniActive = true;
			player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f));

			break;
		}
	}
	ImGui::Separator();

	/// Show Mini Game Data
	ImGui::TextColored(ImVec4(0, 0, 255, 255), "Mini Game Data");

	if (scene->miniGame != nullptr)
	{
		ImGui::Text("Score: %d", scene->miniGame->data->score);
		ImGui::Text("Score Goal: %d", scene->miniGame->data->scoreGoal);
		ImGui::Text("Is Left Goal Active: %s", scene->miniGame->data->isLeftGoalActive ? "True" : "False");
		ImGui::Text("Is Right Goal Active: %s", scene->miniGame->data->isRightGoalActive ? "True" : "False");
		ImGui::InputFloat4("Screen Size: %d", &scene->miniGame->data->screen.x);
		ImGui::InputFloat4("Goal Size: %d", &scene->miniGame->data->goal.x);

		ImGui::Text("Obstacles: %d", scene->miniGame->data->obstacles.size());
		for (auto& obj : scene->miniGame->data->obstacles)
		{
			if (ImGui::Button(std::to_string(obj.x).c_str()))
			{
				ImGui::Text("Object: X %f", obj.x);
				ImGui::Text("Object: Y %f", obj.y);
				ImGui::Text("Object: Width %f", obj.width);
				ImGui::Text("Object: Height %f", obj.height);

			}
		}
	}

	ImGui::End();
}