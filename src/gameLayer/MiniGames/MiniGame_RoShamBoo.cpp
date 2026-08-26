#include "MiniGame_RoShamBoo.h"

#include <SceneManager.h>

MiniGame* MiniGame_RoShamBoo(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "RoShamBoo";
	game->update = &RoShamBoo::update;
	game->draw = &RoShamBoo::render;
	
	game->data->scoreGoal = 5;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.4f, GetScreenHeight() * 0.4f));
	player->rigidBody2D.canMove = false;

	auto rng = std::ranlux24_base(std::random_device{}());
	game->data->scoreGoal = getRandomInt(rng, 1, 3);
	std::cout << game->name << " Goal: " << game->data->scoreGoal << "\n";

	return game;
}

void RoShamBoo::render(Scene* scene_ptr)
{
	auto inputSystem = &InputSystem::getInstance();

	// Display 3 Cards
	auto leftCard = getScreenScale(Rectangle{ 0.1f, 0.3f, 0.2f, 0.5f });
	auto middleCard = getScreenScale(Rectangle{ 0.4f, 0.3f, 0.2f, 0.5f });
	auto rightCard = getScreenScale(Rectangle{ 0.7f, 0.3f, 0.2f, 0.5f });

	// Highlighted from the same actions update() selects on. These were
	// hardcoded to A/S/D, which is only correct while the movement keys sit at
	// their defaults — rebinding them lit up a card the player was not choosing
	// and left the card they were choosing dark.
	DrawRectangleRec(leftCard, inputSystem->IsActionDown(ACTION_MOVE_LEFT) ? YELLOW : RED);
	DrawRectangleRec(middleCard, inputSystem->IsActionDown(ACTION_MOVE_BACKWARD) ? YELLOW : WHITE);
	DrawRectangleRec(rightCard, inputSystem->IsActionDown(ACTION_MOVE_RIGHT) ? YELLOW : BLUE);

}

void RoShamBoo::update(Scene* scene_ptr, float delta)
{
	auto inputSystem = &InputSystem::getInstance();
	auto data = scene_ptr->miniGame->data;

	// Select Left
	if (inputSystem->IsActionPressed(ACTION_MOVE_LEFT)) { data->score = 1; }
	// Select Middle
	if (inputSystem->IsActionPressed(ACTION_MOVE_BACKWARD)) { data->score = 2; }
	// Select Right
	if (inputSystem->IsActionPressed(ACTION_MOVE_RIGHT)) { data->score = 3; }
	
	if (data->score == data->scoreGoal) { 
		CompleteMiniGame(data, scene_ptr->player, BUFF_RANDOM, true);
		scene_ptr->ReleaseMiniGame();
	};
	
	
}
