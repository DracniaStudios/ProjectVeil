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

void Maze::render(MiniGameData* data, void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
}

void Maze::update(MiniGameData* data, void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	auto inputSystem = &InputSystem::getInstance();

	player->getBuff(BUFF_SEARCH)->use();
	if (data->score >= data->scoreGoal) { data->isComplete = true; }
}
