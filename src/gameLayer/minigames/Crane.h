#pragma once
#ifndef CRANE_H
#define CRANE_H

#include <MiniGame.h>

struct Crane : MiniGame
{
	static void render(void* manager_ptr, void* player_ptr);
	static void update(void* manager_ptr, void* player_ptr, float deltaTime);
};

#endif
