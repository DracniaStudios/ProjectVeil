#pragma once
#ifndef MINI_GAME_DOCTOR_H
#define MINI_GAME_DOCTOR_H

#include <MiniGame.h>

struct Doctor : MiniGame
{
	static void render(MiniGameData* data, void* player_ptr);
	static void update(MiniGameData* data, void* player_ptr, float delta);
};

#endif
