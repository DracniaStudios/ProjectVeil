#pragma once
#ifndef MINI_GAME_DOCTOR_H
#define MINI_GAME_DOCTOR_H

#include <MiniGame.h>

struct Doctor : MiniGame
{
	static void render(void* player_ptr);
	static void update(void* player_ptr, float delta);
};

#endif
