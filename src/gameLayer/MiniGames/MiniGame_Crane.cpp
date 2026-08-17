#include "MiniGame_Crane.h"

#include <Player.h>
#include <SceneManager.h>

MiniGame* MiniGame_Crane(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Crane";
	game->update = &Crane::update;
	game->draw = &Crane::render;
	game->data = new MiniGameData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f));
	player->rigidBody2D.scale = Vector3(5, 5, 5);

	auto screen = game->data->screen = getScreenScale({ 0.25f, 0.2f, 0.5f, 0.7f });
	// Generate Obstacles
	{
		game->data->obstacles = {};
		auto rng = std::ranlux24_base(std::random_device{}());
		
		// Generate Obstacles
		for (int i = 0; i < 9; i++)
		{

			// Create Y Offset For Obstacles
			float width = getRandomFloat(rng, 0.1f, 0.4f);
			float height = GetScreenHeight() * 0.03f;
			float x = screen.x + (screen.x * getRandomFloat(rng, 0, 0.5f));
			float y = screen.y + (screen.height * (i * 0.1f));
			//float y = screen.y + (i * height);
			// Gen Top Size
			
			Rectangle left = {
				x,
				y,
				screen.width * width,
				height
			};

			// Gen Bottom Size
			x = screen.x + (screen.x * getRandomFloat(rng, 0.5f, 0.8f));
			width = screen.width * width;
			Rectangle right = {
				(screen.x + screen.width) - width,
				y,
				width,
				height
			};

			game->data->obstacles.push_back(left);
			game->data->obstacles.push_back(right);
		}
	}
	
	/// Goal Line
	game->data->goal = generateScaleRect(game->data->screen, { 1.0f, 1.0f, 1.0f, 0.1f });
	game->data->goal.y = game->data->screen.y + game->data->screen.height - game->data->goal.height;
	return game;
}

void Crane::render(MiniGameData* data, Player* player)
{
	// An unused std::random_device + generator used to be constructed here, once
	// per frame. random_device can hit the OS entropy source on construction, so
	// this was a syscall per frame in the draw path for a value nothing read.

	/// Draw Background Screen
	DrawRectangleRec(data->screen, BLACK);
	DrawRectangleRec(data->goal, BLUE);

	for (auto& obj : data->obstacles)
	{
		DrawRectangleRec(obj, RED);
	}

}

void Crane::update(MiniGameData* data, Player* player, float deltaTime)
{
	auto inputSystem = &InputSystem::getInstance();
	auto scene = SceneManager::getInstance().currentScene;

	/// Player Logic
	{
		player->rigidBody2D.applyGravity(Vector2{ 0, -150 });

		static int speed = 2;

		if (inputSystem->IsActionDown(ACTION_MOVE_JUMP)) { player->rigidBody2D.jump(-200); }
		if (inputSystem->IsActionDown(ACTION_MOVE_LEFT)) { player->rigidBody2D.translation += Vector3(-speed, 0); }
		if (inputSystem->IsActionDown(ACTION_MOVE_RIGHT)) { player->rigidBody2D.translation += Vector3(speed, 0); }

		if (player->rigidBody2D.getPosition().y < data->screen.y)
		{
			player->rigidBody2D.translation = Vector3(player->rigidBody2D.getPosition().x, data->screen.y  + player->rigidBody2D.scale.x);
		}

	}

	for (auto obstacle : data->obstacles)
	{
		if (CheckCollisionCircleRec(player->getPosition2D(), player->rigidBody2D.scale.x, obstacle))
		{
			data->isReset = true;
		}
	}

	/// Goal Logic
	{
		if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, data->goal))
		{
			CompleteMiniGame(data, player, BUFF_RANGE, true);
		}

		auto playerRect = Rectangle(player->rigidBody2D.translation.x, player->rigidBody2D.translation.y, player->rigidBody2D.scale.x, player->rigidBody2D.scale.y);

		if (!CheckCollisionRecs(playerRect, data->screen)) {
			scene->ReleaseMiniGame();
		}
	}

}
