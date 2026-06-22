#pragma once
#ifndef MINI_GAME_RO_SHAM_BOO_H
#define MINI_GAME_RO_SHAM_BOO_H

#include <MiniGame.h>

struct RoShamBoo : MiniGame
{
	static void render(MiniGameData* data, void* player_ptr);
	static void update(MiniGameData* data, void* player_ptr, float deltaTime);
};

#endif
