#include "MiniGame_Doctor.h"

#include <SceneManager.h>

MiniGameData doctorData{
	0,
	1,
	{},
	{},
	{},
};

MiniGame* MiniGame_Doctor(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Doctor";
	game->update = &Doctor::update;
	game->draw = &Doctor::render;
	game->data = &doctorData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.2f));


	return game;
}

void Doctor::render(void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
}

void Doctor::update(void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);

}
