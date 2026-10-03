#pragma once
#ifndef MINI_GAME_RO_SHAM_BOO_H
#define MINI_GAME_RO_SHAM_BOO_H

#include <MiniGame.h>

struct RoShamBoo : MiniGame
{
	static void render(SceneManagement::Scene* scene_ptr);
	static void update(SceneManagement::Scene* scene_ptr, float deltaTime);
};

#endif
