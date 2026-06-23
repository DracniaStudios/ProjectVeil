#include "MiniGame_SimonSays.h"

#include <SceneManager.h>

// Step, Key
std::unordered_map<int, int> simonSaysOrder = {};

MiniGame* MiniGame_SimonSays(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Simon says";
	game->update = &SimonSays::update;
	game->draw = &SimonSays::render;

	game->data = new MiniGameData;
	game->data->scoreGoal = 25;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.2f, GetScreenHeight() * 0.2f));


	auto rng = std::ranlux24_base(std::random_device{}());
	for (int i = 0; i <= game->data->scoreGoal; ++i)
	{
		simonSaysOrder[i] = getRandomInt(rng, 0, 4);
	}

	return game;
}


void SimonSays::render(MiniGameData* data, void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
	for (size_t i = 0; i <= simonSaysOrder.size(); ++i)
	{
		auto rect = getScreenScale(Rectangle{ 0.025f * i, 0.45f, 0.01f, 0.01f });

		DrawRectangleRec(rect, data->score == static_cast<int>(i) ? LIGHTGRAY : BLUE);
		DrawText(std::to_string(simonSaysOrder[i]).c_str(), rect.x, rect.y, 20, WHITE);
		// Show Icon based On Selection
	}

}

void SimonSays::update(MiniGameData* data, void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	
	if (IsKeyPressed(KEY_W) && simonSaysOrder[data->score] == 0) { data->score++; }
	if (IsKeyPressed(KEY_A) && simonSaysOrder[data->score] == 1) { data->score++; }
	if (IsKeyPressed(KEY_S) && simonSaysOrder[data->score] == 2) { data->score++; }
	if (IsKeyPressed(KEY_D) && simonSaysOrder[data->score] == 3) { data->score++; }
	
	if (data->score >= data->scoreGoal) { data->isComplete = true; }

}
