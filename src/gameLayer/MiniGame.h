#pragma once
#ifndef MINIGAME_H
#define MINIGAME_H

#include <iostream>
#include <raylib.h>
#include <vector>

#include <Helpers.h>
#include <randomStuff.h>

struct Scene;
struct Player;

struct MiniGameData
{
	int score = 0;
	int scoreGoal = 1;
	bool isReset = false;
	bool isComplete = false;

	std::vector<Rectangle> obstacles = {};
	Rectangle screen = {};
	Rectangle goal = {};
};

// Update Game Method
typedef void (*updateGameMethod)(MiniGameData* data, void* player_ptr, float deltaTime);
typedef void (*drawGameMethod)(MiniGameData* data, void* player_ptr);

struct MiniGame
{
	const char* name = {};
	updateGameMethod update;
	drawGameMethod draw;
	MiniGameData* data = {};

	void SetGoal(const Rectangle rect) const { data->goal = rect; }
};

/// Mini Game Constructors
#define MINI_GAME_FLAPPY_BIRD_ID = 0
MiniGame* MiniGame_FlappyBird(Player* player);
#define MINI_GAME_CRANE_ID = 1
MiniGame* MiniGame_Crane(Player* player);
#define MINI_GAME_DOCTOR_ID = 2
MiniGame* MiniGame_Doctor(Player* player);
#define MINI_GAME_SIMON_SAYS_ID = 3
MiniGame* MiniGame_SimonSays(Player* player);
#define MINI_GAME_MAZE_ID = 4
MiniGame* MiniGame_Maze(Player* player);
#define MINI_GAME_RO_SHAM_BOO_ID = 5
MiniGame* MiniGame_RoShamBoo(Player* player);// Rock Paper Scissors


/// Convert and store any objects as pixels before rendering
/*** Draw MiniGame ***/
inline Rectangle generateScaleRect(Rectangle screen, Rectangle rect)
{
	Rectangle r = {
		screen.x * rect.x,
		screen.y * rect.y,
		screen.width * rect.width,
		screen.height * rect.height
	};
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

// Generate a Rectangle based on ScreenSize to Percentage
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

#endif
