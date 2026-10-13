#pragma once
#ifndef FLAPPYBIRD_H
#define FLAPPYBIRD_H

#include <MiniGame.h>

struct FlappyBird : MiniGame
{
	static void render(SceneManagement::Scene* scene_ptr);
	static void update(SceneManagement::Scene* scene_ptr, float deltaTime);
};


#endif
