#pragma once
#ifndef MINIGAME_H
#define MINIGAME_H

#include <iostream>
#include <random>
#include <raylib.h>
#include <vector>

#include <randomStuff.h>

struct Scene;
struct Player;

// Update Game Method
typedef void (*updateGameMethod)(void* player_ptr, float deltaTime);
typedef void (*drawGameMethod)(void* player_ptr);

struct MiniGameData
{
	int score = 0;
	int scoreGoal = 0;
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

	void SetGoal(const Rectangle rect) const { data->goal = rect; }
	void Reset() { data = {}; }
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
	Rectangle newSize{};
	newSize.x = 1 + rect.x;
	newSize.y = 1 + rect.y;
	newSize.width = rect.width;
	newSize.height = rect.height;

	return generateScaleRect(screen, newSize);

}

inline Rectangle getScreenScale(Rectangle rect)
{
	Rectangle r{};
	r.x = GetScreenWidth() * rect.x;
	r.y = GetScreenHeight() * rect.y;
	r.width = GetScreenWidth() * rect.width;
	r.height = GetScreenHeight() * rect.height;

	return r;
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

/// Mini Game Constructors
MiniGame* MiniGame_FlappyBird(Player* player);
MiniGame* MiniGame_Crane(Player* player);
MiniGame* MiniGame_Doctor(Player* player);
MiniGame* MiniGame_SimonSays(Player* player);
MiniGame* MiniGame_TimedSimonSays(Player* player);
MiniGame* MiniGame_Maze(Player* player);
MiniGame* MiniGame_RoShamBoo(Player* player);// Rock Paper Scissors

#endif
