#include "Crane.h"

#include <Player.h>
#include <SceneManager.h>

MiniGameData craneData = {
	0,
	1,
	false,
	true,
	{},
	{},
	{}
};

void generateCrane(int count = 4)
{
	craneData.obstacles = {};
	auto rng = std::ranlux24_base(std::random_device{}());

	// x 500 -> 1100
	Rectangle mockScreen = { 0.25f, 0.2f, 0.5f, 0.7f };

	for (int i = 0; i < count; i++)
	{

		float y = i * 0.5f + 0.2f;
		float width = getRandomFloat(rng, 0.1f, 1.0f);

		// Gen Top Size
		Rectangle left = { 0, y, width * 0.4f, 0.05f };

		// Gen Bottom Size
		float rightOffset = 2.0f - width;
		Rectangle right = { rightOffset, left.y, 1 - left.width, left.height };

		craneData.obstacles.push_back(left);
		craneData.obstacles.push_back(right);
	}
}

void Crane::render(void* manager_ptr, void* player_ptr)
{
	std::ranlux24_base rng(std::random_device{}());

	/// Draw Background Screen
	craneData.screen = getScreenScale({ 0.25f, 0.2f, 0.5f, 0.7f });
	DrawRectangleRec(craneData.screen, BLACK);


	/// Goal Line
	craneData.goal = generateScaleRect(craneData.screen, { 1.0f, 1.0f, 1.0f, 0.1f });
	craneData.goal.y = craneData.screen.y + craneData.screen.height - craneData.goal.height;
	DrawRectangleRec(craneData.goal, BLUE);

	for (auto& obj : craneData.obstacles)
	{
		Rectangle newSize = obj;

		newSize.x = 1 + obj.x;
		newSize.y = 1 + obj.y;
		newSize.width = obj.width;
		newSize.height = obj.height;
		// adjust object side accordingly

		DrawRectangleRec(generateScaleRect(craneData.screen, newSize), RED);
	}

}

void Crane::update(void* manager_ptr, void* player_ptr, float deltaTime)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(manager->currentScene->object_ptr);
	auto player = static_cast<Player*>(player_ptr);

	/// Player Logic
	{
		player->rigidBody2D.applyGravity(Vector2{ 0, -150 });

		static int speed = 2;

		if (IsKeyPressed(KEY_SPACE)) { player->rigidBody2D.jump(-200); }
		if (IsKeyDown(KEY_LEFT)) { player->rigidBody2D.translation += Vector3(-speed, 0); }
		if (IsKeyDown(KEY_RIGHT)) { player->rigidBody2D.translation += Vector3(speed, 0); }

		if (player->rigidBody2D.getPosition().y < craneData.screen.y)
		{
			player->rigidBody2D.translation = Vector3(player->rigidBody2D.getPosition().x, craneData.screen.y  + player->rigidBody2D.scale.x);
		}

	}

	/// Goal Logic
	{
		if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, craneData.goal))
		{
			scene->isMiniActive = false;
			player->health += 5;
			std::cout << "Completed Mini Game: Crane \n";
		}
	}

}

MiniGame* MiniGame_crane()
{
	MiniGame* game = new MiniGame();
	game->name = "Crane";
	game->update = &Crane::update;
	game->draw = &Crane::render;
	game->data = &craneData;

	generateCrane(6);

	return game;
}