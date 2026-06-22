#pragma once
#ifndef FLAPPYBIRD_H
#define FLAPPYBIRD_H

#include <MiniGame.h>

struct FlappyBird : MiniGame
{
	static void render(MiniGameData* data, void* player_ptr);
	static void update(MiniGameData* data, void* player_ptr, float deltaTime);
};


#endif
