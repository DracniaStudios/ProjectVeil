#include "FlappyBird.h"

#include <Player.h>
#include <SceneManager.h>


Rectangle leftGoal = {};
bool isLeftGoalActive = false;

Rectangle rightGoal = {};
bool isRightGoalActive = true;

int score = 0;
int scoreGoal = 5;

void FlappyBird::render(void* manager_ptr, void* player_ptr)
{
	std::ranlux24_base rng(std::random_device{}());

	Rectangle screen = {};
	/// Draw Background Screen
	
	screen.x = GetScreenWidth() * 0.25f;
	screen.y = GetScreenHeight() * 0.3f;
	screen.width = GetScreenWidth() * 0.5f;
	screen.height = GetScreenHeight() * 0.5f;
	
	
	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 1, 1, 1 }), BLACK);
	DrawRectangleRec(generateScaleRect(screen, Rectangle{1.05f, 1.05f,0.95f, 0.95f}), DARKGREEN);
	
	/// Draw Goal Borders
	leftGoal = generateScaleRect(screen, Rectangle{ 1.05f, 1.05f, 0.1f, 0.95f });
	rightGoal = generateScaleRect(screen, Rectangle{ 2.75f, 1.05f, 0.1f, 0.95f });


	if (isLeftGoalActive) { DrawRectangleRec(leftGoal, GOLD); }
	if (isRightGoalActive) { DrawRectangleRec(rightGoal, GOLD); }
	
	/// Draw Enemy Tiles

}

void FlappyBird::update(void* manager_ptr, void* player_ptr, float deltaTime)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(manager->currentScene->object_ptr);
	auto player = static_cast<Player*>(player_ptr);

	if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, leftGoal))
	{
		if (!isLeftGoalActive) return;
		std::cout << "Player Inside Left Goal \n";
		score++;
		isRightGoalActive = true;
		isLeftGoalActive = false;

	}
	if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, rightGoal))
	{
		if (!isRightGoalActive) return;
		std::cout << "Player Inside Right Goal \n";
		score++;
		isLeftGoalActive = true;
		isRightGoalActive = false;
	}

	if (score >= scoreGoal)
	{
		scene->isMiniActive = false;
		player->health += 5;
		std::cout << "Completed Mini Game \n";
	}


}

MiniGame* MiniGame_flappyBird()
{
	MiniGame* game = new MiniGame();
	game->update = &FlappyBird::update;
	game->draw = &FlappyBird::render;
	game->isEnabled = true;

	return game;
}