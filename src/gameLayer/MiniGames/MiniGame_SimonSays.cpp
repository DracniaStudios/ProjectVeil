#include "MiniGame_SimonSays.h"

#include <SceneManager.h>

// Step, Key
MiniGame* MiniGame_SimonSays(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Simon says";
	game->update = &SimonSays::update;
	game->draw = &SimonSays::render;

	game->data = new MiniGameData;
	game->data->scoreGoal = 25;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.2f, GetScreenHeight() * 0.2f));
	player->rigidBody2D.canMove = false;

	auto rng = std::ranlux24_base(std::random_device{}());
	for (int i = 0; i < game->data->scoreGoal; ++i)
	{
		// Holds ID and Type. update() only recognizes obstacle values 0-3 (one
		// per direction) — getRandomInt is inclusive of its upper bound, so this
		// must stay 3, not 4, or a generated obstacle could never be cleared.
		game->data->obstacles.push_back(Rectangle{static_cast<float>(i), static_cast<float>(getRandomInt(rng, 0, 3))});
		
	}

	return game;
}


void SimonSays::render(MiniGameData* data, Player* player)
{
	
	for (size_t i = 0; i < data->obstacles.size(); i++)
	{
		auto rect = getScreenScale(Rectangle{ 0.025f * i, 0.45f, 0.01f, 0.01f });
		rect.x += 300;

		DrawRectangleRec(rect, data->score == static_cast<int>(i) ? LIGHTGRAY : BLUE);
		
		DrawText(std::to_string(static_cast<int>(data->obstacles[i].y)).c_str(), rect.x, rect.y, 20, WHITE);
		// Show Icon based On Selection
	}

}

void SimonSays::update(MiniGameData* data, Player* player, float delta)
{
	auto inputSystem = &InputSystem::getInstance();
	
	// A single frame can register more than one key press; each branch re-checks the same
	// index, so chain them with else-if to avoid indexing obstacles[] once score reaches
	// scoreGoal (out-of-bounds) or awarding more than one point per frame.
	if (data->score < static_cast<int>(data->obstacles.size())) {
		if (inputSystem->IsActionPressed(ACTION_MOVE_FORWARD) && data->obstacles[data->score].y == 0) { data->score++; }
		else if (inputSystem->IsActionPressed(ACTION_MOVE_LEFT) && data->obstacles[data->score].y == 1) { data->score++; }
		else if (inputSystem->IsActionPressed(ACTION_MOVE_BACKWARD) && data->obstacles[data->score].y == 2) { data->score++; }
		else if (inputSystem->IsActionPressed(ACTION_MOVE_RIGHT) && data->obstacles[data->score].y == 3) { data->score++; }
	}
	CompleteMiniGame(data, player, BUFF_HEARING);

}
