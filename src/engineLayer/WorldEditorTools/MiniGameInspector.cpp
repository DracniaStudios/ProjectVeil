#include <WorldEditor.h>

void WorldEditor::ShowMiniGameData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Begin Mini Game Data Window
	ImGui::Begin("Mini Game Data");

	ImGui::Text("Display Mini Game Data");
	ImGui::InputInt("Mini Game ID", &currentGameID, 1, 1);

	// Upper bound was 6, one past the last id. Nothing handled 6, so it fell
	// through to the default branch and quietly launched Flappy Bird while the
	// field still read "6".
	currentGameID = Clamp(currentGameID, MINI_GAME_FLAPPY_BIRD_ID, MINI_GAME_RO_SHAM_BOO_ID);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", miniGameIdToString(currentGameID));

	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);

	/// Launch Mini Game Button
	if (ImGui::Button("Launch Mini Game"))
	{
		miniGameObstacleIndex = -1; // Selection belongs to the previous game's obstacle list

		// Route through Scene::SetMiniGame rather than constructing the game
		// here. The open-coded switch this replaces built the MiniGame but left
		// is2DActive false, isMiniActive stale and lastMiniGamePlayed pointing at
		// the previous game — so the launched game never drew (Scene_drawScene2D
		// gates the minigame layer on is2DActive) and losing it replayed the
		// wrong one. SetMiniGame also owns freeing the game being replaced.
		scene->SetMiniGame(currentGameID);
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
		if (ImGui::Button("Complete")) { CompleteMiniGame(*scene->miniGame->data, *scene->player, scene->gameMap, BUFF_RANGE, true); }
		if (ImGui::Button("Reset")) { scene->ResetMiniGame(); }
		if (ImGui::Button("Release")) { scene->ReleaseMiniGame(); }
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