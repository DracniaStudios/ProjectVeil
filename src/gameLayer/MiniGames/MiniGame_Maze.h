#pragma once
#ifndef MINI_GAME_MAZE_H
#define MINI_GAME_MAZE_H

#include <MiniGame.h>

struct Maze : MiniGame
{
	static void render(Scene* scene_ptr);
	static void update(Scene* scene_ptr, float deltaTime);
};

#endif
