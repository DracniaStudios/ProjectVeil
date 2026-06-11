#pragma once
#ifndef MINI_GAME_SIMON_SAYS_H
#define MINI_GAME_SIMON_SAYS_H

#include <MiniGame.h>

struct SimonSays : MiniGame
{
	static void render(void* player_ptr);
	static void update(void* player_ptr, float deltaTime);
};

#endif
