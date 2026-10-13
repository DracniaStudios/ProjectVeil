#pragma once
#ifndef CRANE_H
#define CRANE_H

#include <MiniGame.h>

struct Crane : MiniGame
{
	static void render(SceneManagement::Scene* scene_ptr);
	static void update(SceneManagement::Scene* scene_ptr, float deltaTime);
};

#endif
