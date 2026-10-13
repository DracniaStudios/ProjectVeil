#pragma once
#ifndef MINI_GAME_DOCTOR_H
#define MINI_GAME_DOCTOR_H

#include <MiniGame.h>

struct Doctor : MiniGame
{
	static void render(SceneManagement::Scene* scene_ptr);
	static void update(SceneManagement::Scene* scene_ptr, float delta);
};

#endif
