#include "MiniGame_Maze.h"

#include <SceneManager.h>

MiniGame* MiniGame_Maze(Player* player)
{
	auto* game = new MiniGame();
	game->name = "Maze";
	game->update = &Maze::update;
	game->draw = &Maze::render;
	game->data = new MiniGameData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.1f, GetScreenHeight() * 0.1f));

	return game;
}

void Maze::render(MiniGameData* data, Player* player)
{
	
}

void Maze::update(MiniGameData* data, Player* player, float delta)
{
	auto inputSystem = &InputSystem::getInstance();

	CompleteMiniGame(data, player, BUFF_SEARCH);
}
