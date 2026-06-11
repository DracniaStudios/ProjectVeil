#include "MiniGame_Maze.h"

#include <SceneManager.h>

MiniGameData mazeData{
	0,
	1,
	{},
	{},
	{},
};

MiniGame* MiniGame_Maze(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Maze";
	game->update = &Maze::update;
	game->draw = &Maze::render;
	game->data = &mazeData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.1f, GetScreenHeight() * 0.1f));

	return game;
}

void Maze::render(void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
}

void Maze::update(void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);

}
