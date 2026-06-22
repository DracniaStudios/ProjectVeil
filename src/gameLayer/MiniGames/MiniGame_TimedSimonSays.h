#pragma once
#ifndef MINI_GAME_TIMED_SIMON_SAYS_H
#define MINI_GAME_TIMED_SIMON_SAYS_H

#include <MiniGame.h>

struct TimedSimonSays : MiniGame
{
	static void render(MiniGameData* data, void* player_ptr);
	static void update(MiniGameData* data, void* player_ptr, float deltaTime);
};

#endif
