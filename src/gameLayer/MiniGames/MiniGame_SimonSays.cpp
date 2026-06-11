#include "MiniGame_SimonSays.h"

#include <SceneManager.h>

MiniGameData SimonSays{
	0,
	1,
	{},
	{},
	{},
};

MiniGame* MiniGame_SimonSays(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Simon says";
	game->update = &SimonSays::update;
	game->draw = &SimonSays::render;
	game->data = &SimonSays;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.2f, GetScreenHeight() * 0.2f));

	return game;
}

void SimonSays::render(void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
}

void SimonSays::update(void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	


}
