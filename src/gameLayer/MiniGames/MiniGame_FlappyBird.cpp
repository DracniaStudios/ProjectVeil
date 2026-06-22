#include "MiniGame_FlappyBird.h"

#include <SceneManager.h>

Rectangle leftGoal = {};
bool isLeftGoalActive = false;

Rectangle rightGoal = {};
bool isRightGoalActive = true;

MiniGame* MiniGame_FlappyBird(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Flappy Bird";
	game->update = &FlappyBird::update;
	game->draw = &FlappyBird::render;
	game->data = new MiniGameData;

	game->data->scoreGoal = 5;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.2f));

	// Generate Obstacle
	{
		game->data->obstacles = {};
		auto rng = std::ranlux24_base(std::random_device{}());

		// x 500 -> 1100

		for (int i = 0; i < 4; i++)
		{
			// Gen Top Size
			Rectangle top = { getRandomFloat(rng, 0.2f, 1.5f), 0.05f, 0.05f, getRandomFloat(rng, 0.1f, 0.3f) };

			// Gen Bottom Size
			Rectangle bottom = { top.x, top.height + 0.9f, top.width, 0.5f - top.height };

			game->data->obstacles.push_back(top);
			game->data->obstacles.push_back(bottom);
		}
	}

	return game;
}

void FlappyBird::render(MiniGameData* data, void* player_ptr)
{
	auto rng(std::random_device{}());
	/// Draw Background Screen
	Rectangle screen = getScreenScale({ 0.25f, 0.3f, 0.5f, 0.5f });
	
	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 1, 1, 1 }), BLACK);
	DrawRectangleRec(generateScaleRect(screen, Rectangle{1.05f, 1.05f,0.95f, 0.95f}), DARKGREEN);
	
	/// Draw Goal Borders
	leftGoal = generateScaleRect(screen, Rectangle{ 1.05f, 1.05f, 0.1f, 0.95f });
	rightGoal = generateScaleRect(screen, Rectangle{ 2.75f, 1.05f, 0.1f, 0.95f });


	if (isLeftGoalActive) { DrawRectangleRec(leftGoal, GOLD); }
	if (isRightGoalActive) { DrawRectangleRec(rightGoal, GOLD); }
	
	/// Draw Enemy Tiles

	for (auto &obj : data->obstacles)
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

void FlappyBird::update(MiniGameData* data, void* player_ptr, float deltaTime)
{
	auto scene = SceneManager::getInstance().currentScene;
	auto player = static_cast<Player*>(player_ptr);

	auto generateObstacle = [&](int count = 4)
		{
			data->obstacles = {};
			auto rng = std::ranlux24_base(std::random_device{}());

			// x 500 -> 1100

			for (int i = 0; i < count; i++)
			{
				// Gen Top Size
				Rectangle top = { getRandomFloat(rng, 0.2f, 1.5f), 0.05f, 0.05f, getRandomFloat(rng, 0.1f, 0.3f) };

				// Gen Bottom Size
				Rectangle bottom = { top.x, top.height + 0.9f, top.width, 0.5f - top.height };

				data->obstacles.push_back(top);
				data->obstacles.push_back(bottom);
			}
		};

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
		data->score++;
		isRightGoalActive = true;
		isLeftGoalActive = false;
		player->rigidBody2D.velocity = {};
		generateObstacle(2 + data->score);

	}
	if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, rightGoal))
	{
		if (!isRightGoalActive) return;
		std::cout << "Player Inside Right Goal \n";
		data->score++;
		isLeftGoalActive = true;
		isRightGoalActive = false;
		player->rigidBody2D.velocity = {};
		generateObstacle(2 + data->score);
	}

	if (player->rigidBody2D.getPosition().x < screen.x) scene->miniGame->Reset();
	if (player->rigidBody2D.getPosition().y < screen.y) scene->miniGame->Reset();
	if (player->rigidBody2D.getPosition().x > screen.x + screen.width) scene->miniGame->Reset();
	if (player->rigidBody2D.getPosition().y > screen.y + screen.height) scene->miniGame->Reset();

	// Win Condition

	if (data->score >= data->scoreGoal)
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
		for (auto &obj : data->obstacles)
		{
			Rectangle newSize{
				1 + obj.x,
				1 + obj.y,
				obj.width,
				obj.height,
			};

			Rectangle obstacle = generateScaleRect(screen, newSize);


			ImGui::Text(std::to_string(obstacle.y).c_str());
			if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, obstacle)) scene->miniGame->Reset();
		}
	}
}
