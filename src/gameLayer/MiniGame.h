#pragma once
#ifndef MINIGAME_H
#define MINIGAME_H

#include <raylib.h>

struct Scene;
struct Player;

// Update Game Method
typedef void (*updateGameMethod)(void* manager_ptr, void* player_ptr, float deltaTime);
typedef void (*drawGameMethod)(void* manager_ptr, void* scene_ptr);

struct MiniGame
{
	const char* name = {};
	Rectangle screen = {};
	updateGameMethod update;
	drawGameMethod draw;
};

// Angel Engine Style
inline Rectangle generateScaleRect(Rectangle screen, Rectangle rect)
{
	Rectangle r = {};

	r.x = screen.x * rect.x;
	r.y = screen.y * rect.y;
	r.width = screen.width * rect.width;
	r.height = screen.height * rect.height;
	return r;
}

#define MINI_GAME_ID 0
MiniGame* MiniGame_flappyBird();
#define MINI_GAME_ID 1
MiniGame* MiniGame_crane();
/*
#define MINI_GAME_ID 2
MiniGame* MiniGame_doctor();
#define MINI_GAME_ID 3
MiniGame* MiniGame_saysMe();
#define MINI_GAME_ID 4
MiniGame* MiniGame_timedSaysMe();
#define MINI_GAME_ID 5
MiniGame* MiniGame_maze();

// Dokapon Kingdom Style

#define MINI_GAME_ID 6
MiniGame* MiniGame_roShamBoo();// Rock Paper Scissors
*/
#endif
