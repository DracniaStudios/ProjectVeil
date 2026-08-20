#pragma once
#ifndef FLAPPYBIRD_H
#define FLAPPYBIRD_H

#include <MiniGame.h>

struct FlappyBird : MiniGame
{
	static void render(Scene* scene_ptr);
	static void update(Scene* scene_ptr, float deltaTime);
};


#endif
