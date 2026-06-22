#pragma once
#ifndef MINI_GAME_MAZE_H
#define MINI_GAME_MAZE_H

#include <MiniGame.h>

struct Maze : MiniGame
{
	static void render(MiniGameData* data, void* player_ptr);
	static void update(MiniGameData* data, void* player_ptr, float deltaTime);
};

#endif
