#include <WorldEditor.h>

namespace Editor {

void WorldEditor::ShowMiniGameData(Player* player)
{
	auto scene = editedScene;

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

	ImGui::Text("Overlay Open: %s", scene->overlays.IsOpen(Overlay::MiniGame) ? "Yes" : "No");

	/// Launch Mini Game Button
	if (ImGui::Button("Launch Mini Game"))
	{
		miniGameObstacleIndex = -1; // Selection belongs to the previous game's obstacle list

		// Through the Scene, so the overlay opens and the game it replaces ends
		// properly. Launched from here it is a Practice run: no station to complete.
		scene->StartMiniGame(currentGameID);
	}
	ImGui::Separator();

	/// Show Mini Game Data
	ImGui::TextColored(ImVec4(0.0f, 0.0f, 1.0f, 1.0f), "Mini Game Data");

	if (MiniGame* game = scene->miniGameSession.Game())
	{
		ImGui::Text("%s, for station %llu", game->name, static_cast<unsigned long long>(scene->miniGameSession.StationId()));
		ImGui::Text("Score: %d", game->data.score);
		ImGui::Text("Score Goal: %d", game->data.scoreGoal);
		ImGui::InputFloat4("Screen Size: %d", &game->data.screen.x);
		ImGui::InputFloat4("Goal Size: %d", &game->data.goal.x);
		ImGui::Text("Cursor: (%.1f, %.1f)  velocity (%.1f, %.1f)",
			game->cursor.translation.x, game->cursor.translation.y, game->cursor.velocity.x, game->cursor.velocity.y);

		// Win is applied exactly as a real win: the buff, and at a station its
		// completion and Target. Retry is a fresh round, as after a loss.
		if (ImGui::Button("Win")) { scene->WinMiniGame(); }
		ImGui::SameLine();
		if (ImGui::Button("Retry")) { scene->RetryMiniGame(); miniGameObstacleIndex = -1; }
		ImGui::SameLine();
		if (ImGui::Button("Quit")) { scene->QuitMiniGame(); }
		ImGui::Separator();

		// Win and Quit end the game, and Retry replaces it, in this same frame, so
		// the game fetched above can't be used past the buttons
		game = scene->miniGameSession.Game();
		if (game == nullptr)
		{
			ImGui::End();
			return;
		}

		auto& obstacles = game->data.obstacles;
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
}
