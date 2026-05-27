#include "FlappyBird.h"

#include <Player.h>
#include <SceneManager.h>


Rectangle leftGoal = {};
bool isLeftGoalActive = false;

Rectangle rightGoal = {};
bool isRightGoalActive = true;

int score = 0;
int scoreGoal = 5;

std::vector<Rectangle> obstacles;

void Reset(Scene* scene)
{
	std::cout << "Reset Mini Game \n";
	// Reset
	scene->miniGame = {};
	scene->isMiniActive = false;

	// Display Fail Screen For 3 Seconds;

}

void generateObstacle(int count = 4)
{
	obstacles = {};
	auto rng = std::ranlux24_base(std::random_device{}());
	
	// x 500 -> 1100
	
	for (int i = 0; i < count; i++)
	{
		// Gen Top Size
		Rectangle top = {getRandomFloat(rng, 0.2f, 1.5f), 0.05f, 0.05f, getRandomFloat(rng, 0.1f, 0.3f)};

		// Gen Bottom Size
		Rectangle bottom = { top.x, top.height + 0.9f, top.width, 0.5f - top.height};

		obstacles.push_back(top);
		obstacles.push_back(bottom);
	}
}

void FlappyBird::render(void* manager_ptr, void* player_ptr)
{
	std::ranlux24_base rng(std::random_device{}());

	/// Draw Background Screen
	Rectangle screen = {};
	
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

	for (auto &obj : obstacles)
	{
		Rectangle newSize = obj;

		newSize.x = 1 + obj.x;
		newSize.y = 1 + obj.y;
		newSize.width = obj.width;
		newSize.height = obj.height;
		// adjust object side accordingly

		DrawRectangleRec(generateScaleRect(screen, newSize), RED);
	}

	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 1, 1, 0.1f }), BLACK);
	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 2.5f, 1, 0.1f }), BLACK);

}

void FlappyBird::update(void* manager_ptr, void* player_ptr, float deltaTime)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(manager->currentScene->object_ptr);
	auto player = static_cast<Player*>(player_ptr);

	Rectangle screen = {};

	screen.x = GetScreenWidth() * 0.25f;
	screen.y = GetScreenHeight() * 0.3f;
	screen.width = GetScreenWidth() * 0.5f;
	screen.height = GetScreenHeight() * 0.5f;

	// Game Logic
	if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, leftGoal))
	{
		if (!isLeftGoalActive) return;
		std::cout << "Player Inside Left Goal \n";
		score++;
		isRightGoalActive = true;
		isLeftGoalActive = false;
		player->rigidBody2D.velocity = {};
		generateObstacle(2 + score);

	}
	if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, rightGoal))
	{
		if (!isRightGoalActive) return;
		std::cout << "Player Inside Right Goal \n";
		score++;
		isLeftGoalActive = true;
		isRightGoalActive = false;
		player->rigidBody2D.velocity = {};
		generateObstacle(2 + score);
	}

	if (player->rigidBody2D.getPosition().x < screen.x) Reset(scene);
	if (player->rigidBody2D.getPosition().y < screen.y) Reset(scene);
	if (player->rigidBody2D.getPosition().x > screen.x + screen.width) Reset(scene);
	if (player->rigidBody2D.getPosition().y > screen.y + screen.height) Reset(scene);

	// Win Condition

	if (score >= scoreGoal)
	{
		scene->isMiniActive = false;
		player->health += 5;
		std::cout << "Completed Mini Game \n";
	}

	// Player Logic
	{
		static int speed = 15;
		
		player->rigidBody2D.applyGravity();
		if (IsKeyPressed(KEY_SPACE)) { player->rigidBody2D.jump(50); }

		if (isLeftGoalActive) { player->rigidBody2D.applyForce(Vector2(-speed, 0)); }
		if (isRightGoalActive) { player->rigidBody2D.applyForce(Vector2(speed, 0)); }

	}

	// Entity Logic
	{
		ImGui::Begin("Obstacles");
		for (auto &obj : obstacles)
		{
			Rectangle newSize = obj;

			newSize.x = 1 + obj.x;
			newSize.y = 1 + obj.y;
			newSize.width = obj.width;
			newSize.height = obj.height;

			Rectangle obstacle = generateScaleRect(screen, newSize);


			ImGui::Text(std::to_string(obstacle.y).c_str());
			if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, obstacle)) Reset(scene);
		}
		ImGui::End();
	}
}

MiniGame* MiniGame_flappyBird()
{
	MiniGame* game = new MiniGame();
	game->name = "Flappy Bird";
	game->update = &FlappyBird::update;
	game->draw = &FlappyBird::render;

	generateObstacle(2);

	return game;
}