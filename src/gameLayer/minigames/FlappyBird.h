#pragma once
#ifndef FLAPPYBIRD_H
#define FLAPPYBIRD_H

#include <MiniGame.h>

struct FlappyBird : MiniGame
{
	static void render(void* manager_ptr, void* player_ptr);
	static void update(void* manager_ptr, void* player_ptr, float deltaTime);
};


#endif
