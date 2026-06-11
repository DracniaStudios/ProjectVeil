#include "MiniGame_RoShamBoo.h"

#include <SceneManager.h>

MiniGameData roShamBooData{
	0,
	1,
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

	return game;
}

void RoShamBoo::render(void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
}

void RoShamBoo::update(void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	


}
