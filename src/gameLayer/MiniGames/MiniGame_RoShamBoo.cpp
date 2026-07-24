#include "MiniGame_RoShamBoo.h"

#include <SceneManager.h>

MiniGame* MiniGame_RoShamBoo(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "RoShamBoo";
	game->update = &RoShamBoo::update;
	game->draw = &RoShamBoo::render;
	
	game->data = new MiniGameData;
	game->data->scoreGoal = 5;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.4f, GetScreenHeight() * 0.4f));

	auto rng = std::ranlux24_base(std::random_device{}());
	game->data->scoreGoal = getRandomInt(rng, 1, 3);
	std::cout << game->name << " Goal: " << game->data->scoreGoal << "\n";

	return game;
}

void RoShamBoo::render(MiniGameData* data, Player* player)
{
	// Display 3 Cards
	auto leftCard = getScreenScale(Rectangle{ 0.1f, 0.3f, 0.2f, 0.5f });
	auto middleCard = getScreenScale(Rectangle{ 0.4f, 0.3f, 0.2f, 0.5f });
	auto rightCard = getScreenScale(Rectangle{ 0.7f, 0.3f, 0.2f, 0.5f });

	DrawRectangleRec(leftCard, IsKeyDown(KEY_A) ? YELLOW : RED);
	DrawRectangleRec(middleCard, IsKeyDown(KEY_S) ? YELLOW : WHITE);
	DrawRectangleRec(rightCard, IsKeyDown(KEY_D) ? YELLOW : BLUE);

}

void RoShamBoo::update(MiniGameData* data, Player* player, float delta)
{
	auto inputSystem = &InputSystem::getInstance();

	// Select Left
	if (inputSystem->IsActionPressed(ACTION_MOVE_LEFT)) { data->score = 1; }
	// Select Middle
	if (inputSystem->IsActionPressed(ACTION_MOVE_BACKWARD)) { data->score = 2; }
	// Select Right
	if (inputSystem->IsActionPressed(ACTION_MOVE_RIGHT)) { data->score = 3; }
	
	if (data->score == data->scoreGoal) { CompleteMiniGame(data, player, BUFF_RANDOM, true); };
	
	
}
