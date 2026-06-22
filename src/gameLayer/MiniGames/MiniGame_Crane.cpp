#include "MiniGame_Crane.h"

#include <Player.h>
#include <SceneManager.h>

void Crane::render(MiniGameData* data, void* player_ptr)
{
	std::ranlux24_base rng(std::random_device{}());

	/// Draw Background Screen
	data->screen = getScreenScale({ 0.25f, 0.2f, 0.5f, 0.7f });
	DrawRectangleRec(data->screen, BLACK);


	/// Goal Line
	data->goal = generateScaleRect(data->screen, { 1.0f, 1.0f, 1.0f, 0.1f });
	data->goal.y = data->screen.y + data->screen.height - data->goal.height;
	DrawRectangleRec(data->goal, BLUE);

	for (auto& obj : data->obstacles)
	{
		Rectangle newSize = obj;

		newSize.x = 1 + obj.x;
		newSize.y = 1 + obj.y;
		newSize.width = obj.width;
		newSize.height = obj.height;
		// adjust object side accordingly

		DrawRectangleRec(generateScaleRect(data->screen, newSize), RED);
	}

}

void Crane::update(MiniGameData* data, void* player_ptr, float deltaTime)
{
	auto manager = &SceneManager::getInstance();
	auto scene = manager->currentScene;
	auto player = static_cast<Player*>(player_ptr);

	/// Player Logic
	{
		player->rigidBody2D.applyGravity(Vector2{ 0, -150 });

		static int speed = 2;

		if (IsKeyPressed(KEY_SPACE)) { player->rigidBody2D.jump(-200); }
		if (IsKeyDown(KEY_LEFT)) { player->rigidBody2D.translation += Vector3(-speed, 0); }
		if (IsKeyDown(KEY_RIGHT)) { player->rigidBody2D.translation += Vector3(speed, 0); }

		if (player->rigidBody2D.getPosition().y < data->screen.y)
		{
			player->rigidBody2D.translation = Vector3(player->rigidBody2D.getPosition().x, data->screen.y  + player->rigidBody2D.scale.x);
		}

	}

	/// Goal Logic
	{
		if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, data->goal))
		{
			scene->isMiniActive = false;
			player->health += 5;
			std::cout << "Completed Mini Game: Crane \n";
		}
	}

}

MiniGame* MiniGame_Crane(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Crane";
	game->update = &Crane::update;
	game->draw = &Crane::render;
	game->data = new MiniGameData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f));

	// Generate Obstacles
	{
		game->data->obstacles = {};
		auto rng = std::ranlux24_base(std::random_device{}());
		// x 500 -> 1100
		for (int i = 0; i < 6; i++)
		{
			float y = i * 0.5f + 0.2f;
			float width = getRandomFloat(rng, 0.1f, 1.0f);

			// Gen Top Size
			Rectangle left = { 0, y, width * 0.4f, 0.05f };

			// Gen Bottom Size
			float rightOffset = 2.0f - width;
			Rectangle right = { rightOffset, left.y, 1 - left.width, left.height };

			game->data->obstacles.push_back(left);
			game->data->obstacles.push_back(right);
		}
	}
	return game;
}