#pragma once
#ifndef MINI_GAME_SIMON_SAYS_H
#define MINI_GAME_SIMON_SAYS_H

#include <MiniGame.h>

struct SimonSays : MiniGame
{
	static void render(Scene* scene_ptr);
	static void update(Scene* scene_ptr, float deltaTime);
};

#endif
