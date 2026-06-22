#pragma once
#ifndef CRANE_H
#define CRANE_H

#include <MiniGame.h>

struct Crane : MiniGame
{
	static void render(MiniGameData* data, void* player_ptr);
	static void update(MiniGameData* data, void* player_ptr, float deltaTime);
};

#endif
