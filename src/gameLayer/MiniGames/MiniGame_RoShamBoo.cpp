#include "MiniGame_RoShamBoo.h"

#include <SceneManager.h>

MiniGameData roShamBooData{
	0,
	5,
	{},
	{},
	{},
};

MiniGame* MiniGame_RoShamBoo(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "RoShamBoo";
	game->update = &RoShamBoo::update;
	game->draw = &RoShamBoo::render;
	game->data = &roShamBooData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.4f, GetScreenHeight() * 0.4f));

	auto rng = std::ranlux24_base(std::random_device{}());
	roShamBooData.scoreGoal = getRandomInt(rng, 1, 3);
	std::cout << game->name << " Goal: " << roShamBooData.scoreGoal << "\n";

	return game;
}

void RoShamBoo::render(void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);

	// Display 3 Cards
	auto leftCard = getScreenScale(Rectangle{ 0.1f, 0.3f, 0.2f, 0.5f });
	auto middleCard = getScreenScale(Rectangle{ 0.4f, 0.3f, 0.2f, 0.5f });
	auto rightCard = getScreenScale(Rectangle{ 0.7f, 0.3f, 0.2f, 0.5f });

	DrawRectangleRec(leftCard, IsKeyDown(KEY_A) ? YELLOW : RED);
	DrawRectangleRec(middleCard, IsKeyDown(KEY_S) ? YELLOW : WHITE);
	DrawRectangleRec(rightCard, IsKeyDown(KEY_D) ? YELLOW : BLUE);

}

void RoShamBoo::update(void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);

	// Select Left
	if (IsKeyPressed(KEY_A)) { roShamBooData.score = 1; }
	// Select Middle
	if (IsKeyPressed(KEY_S)) { roShamBooData.score = 2; }
	// Select Right
	if (IsKeyPressed(KEY_D)) { roShamBooData.score = 3; }



}
