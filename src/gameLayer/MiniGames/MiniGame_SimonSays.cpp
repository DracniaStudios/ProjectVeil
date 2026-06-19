#include "MiniGame_SimonSays.h"

#include <SceneManager.h>

MiniGameData simonSaysData{
	0,
	25,
	{},
	{},
	{},
};

// Step, Key
std::unordered_map<int, int> simonSaysOrder = {};

MiniGame* MiniGame_SimonSays(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Simon says";
	game->update = &SimonSays::update;
	game->draw = &SimonSays::render;
	game->data = &simonSaysData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.2f, GetScreenHeight() * 0.2f));


	auto rng = std::ranlux24_base(std::random_device{}());
	for (int i = 0; i <= simonSaysData.scoreGoal; ++i)
	{
		simonSaysOrder[i] = getRandomInt(rng, 0, 4);
	}

	return game;
}


void SimonSays::render(void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
	for (size_t i = 0; i <= simonSaysOrder.size(); ++i)
	{
		auto rect = getScreenScale(Rectangle{ 0.025f * i, 0.45f, 0.01f, 0.01f });

		DrawRectangleRec(rect, simonSaysData.score == static_cast<int>(i) ? LIGHTGRAY : BLUE);
		DrawText(std::to_string(simonSaysOrder[i]).c_str(), rect.x, rect.y, 20, WHITE);
		// Show Icon based On Selection
	}

}

void SimonSays::update(void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	
	if (IsKeyPressed(KEY_W) && simonSaysOrder[simonSaysData.score] == 0) { simonSaysData.score++; }
	if (IsKeyPressed(KEY_A) && simonSaysOrder[simonSaysData.score] == 1) { simonSaysData.score++; }
	if (IsKeyPressed(KEY_S) && simonSaysOrder[simonSaysData.score] == 2) { simonSaysData.score++; }
	if (IsKeyPressed(KEY_D) && simonSaysOrder[simonSaysData.score] == 3) { simonSaysData.score++; }
	

}
