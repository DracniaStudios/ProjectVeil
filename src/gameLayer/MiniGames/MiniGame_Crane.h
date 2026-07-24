#pragma once
#ifndef CRANE_H
#define CRANE_H

#include <MiniGame.h>

struct Crane : MiniGame
{
	static void render(MiniGameData* data, Player* player_ptr);
	static void update(MiniGameData* data, Player* player_ptr, float deltaTime);
};

#endif
