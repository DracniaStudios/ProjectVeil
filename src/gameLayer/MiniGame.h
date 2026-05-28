#pragma once
#ifndef MINIGAME_H
#define MINIGAME_H

#include <iostream>
#include <random>
#include <raylib.h>
#include <vector>

#include "randomStuff.h"

struct Scene;
struct Player;

// Update Game Method
typedef void (*updateGameMethod)(void* manager_ptr, void* player_ptr, float deltaTime);
typedef void (*drawGameMethod)(void* manager_ptr, void* scene_ptr);

struct MiniGameData
{
	int score = 0;
	int scoreGoal = 0;
	bool isLeftGoalActive = false;
	bool isRightGoalActive = false;
	std::vector<Rectangle> obstacles = {};
	Rectangle screen = {};
	Rectangle goal = {};
};

struct MiniGame
{
	const char* name = {};
	updateGameMethod update;
	drawGameMethod draw;
	MiniGameData* data;
	bool isReset;
};


inline Rectangle generateScaleRect(Rectangle screen, Rectangle rect)
{
	Rectangle r = {};

	r.x = screen.x * rect.x;
	r.y = screen.y * rect.y;
	r.width = screen.width * rect.width;
	r.height = screen.height * rect.height;
	return r;
}

inline Rectangle generateObstacleRect(Rectangle screen, Rectangle rect)
{
	Rectangle newSize;
	newSize.x = 1 + rect.x;
	newSize.y = 1 + rect.y;
	newSize.width = rect.width;
	newSize.height = rect.height;

	return generateScaleRect(screen, newSize);

}

inline Rectangle getScreenScale(Rectangle rect)
{
	Rectangle r;
	r.x = GetScreenWidth() * rect.x;
	r.y = GetScreenHeight() * rect.y;
	r.width = GetScreenWidth() * rect.width;
	r.height = GetScreenHeight() * rect.height;

	return r;
}

inline void Reset(void* miniGame_ptr)
{
	std::cout << "Reset Mini Game \n";
	// Reset
	auto miniGame = static_cast<MiniGame*>(miniGame_ptr);

	miniGame->update = {};
	miniGame->draw = {};
	miniGame->isReset = true;
}

// The Range is determined by the percentage of the screen (0.2, 0.5, 0.3, 0.1) (x, y, width, height)
inline void generateObstacleHorizontal(MiniGameData* data,Rectangle range, int count = 4)
{
	data->obstacles = {};
	auto rng = std::ranlux24_base(std::random_device{}());

	for (int i = 0; i < count; ++i)
	{
		// Generate Obstacle logic here
		float leftY = getRandomFloat(rng, range.x, range.width);
		float leftHeight = getRandomFloat(rng, range.y, range.height);
	
		Rectangle left = { 0.0f, leftY, 0.05f, leftHeight };
		data->obstacles.push_back(left);

		Rectangle right = { 0.95f, leftY, 0.05f, leftHeight };
		data->obstacles.push_back(right);
	}
}
// The Range is determined by the percentage of the screen (0.2, 0.5, 0.3, 0.1) (x, y, width, height)
inline void generateObstacleVertical(MiniGameData* data, Rectangle range, int count = 4)
{
	data->obstacles = {};
	auto rng = std::ranlux24_base(std::random_device{}());

	for (int i = 0; i < count; ++i)
	{
		// Generate obstacle logic here
		// .2, 1.5, .1, .3
		float topY = getRandomFloat(rng, range.x, range.y);
		float topHeight = getRandomFloat(rng, range.width, range.height);

		Rectangle top = { 0, topY, 0.05f, topHeight };

		// Gen Bottom Size
		Rectangle bottom = { top.x, top.height + 0.1f, top.width, 1.0f - top.height };

		data->obstacles.push_back(top);

		data->obstacles.push_back(bottom);

	}


}



MiniGame* MiniGame_flappyBird();
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
