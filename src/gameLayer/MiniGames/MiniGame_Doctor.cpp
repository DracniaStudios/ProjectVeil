#include "MiniGame_Doctor.h"

#include <SceneManager.h>

MiniGameData doctorData{
	0,
	1,
	{},
	{},
	{},
};

Vector2 doctor_playerSpeed = Vector2{ 10, 10 };
MiniGame* MiniGame_Doctor(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Doctor";
	game->update = &Doctor::update;
	game->draw = &Doctor::render;
	game->data = &doctorData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f));
	player->rigidBody2D.isEnabled = false;
	player->rigidBody2D.maxSpeed = 20;
	player->rigidBody2D.scale = Vector3(10, 10, 10);

	doctorData.screen = getScreenScale({ 0.25f, 0.3f, 0.5f, 0.5f });

	auto rng = std::ranlux24_base(std::random_device{}());
	doctorData.goal = getScreenScale(Rectangle{
		getRandomFloat(rng, 0.3f, 0.7f),
		getRandomFloat(rng, 0.35f, 0.75f),
		10,
		10
	});
	doctorData.goal.width = 30;
	doctorData.goal.height = 30;
	
	Vector2 cubeSize = { 10, 10 };
	auto xCube = Rectangle{ doctorData.screen.x + cubeSize.x, doctorData.screen.y + cubeSize.y, cubeSize.x, cubeSize.y, };
	doctorData.obstacles.push_back(xCube);
	auto yCube = Rectangle{ doctorData.screen.x + cubeSize.x, doctorData.screen.y + cubeSize.y, cubeSize.x, cubeSize.y, };
	doctorData.obstacles.push_back(yCube);
	return game;
}

void Doctor::render(void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	auto screen = doctorData.screen;

	DrawRectangleRec(screen, BLACK);
	DrawRectangleRec(doctorData.goal, GREEN);

	if (auto obj = &doctorData.obstacles.front())
	{
		obj->x = player->getPosition2D().x - obj->width * 0.5f;
	}
	if (auto obj = &doctorData.obstacles[1])
	{
		obj->y = player->getPosition2D().y - obj->height * 0.5f;
	}

	// 2 Pointer Cubes align to the Player's X and Y
	for (auto obj : doctorData.obstacles)
	{
		DrawRectangleRec(obj, DARKBLUE);
	}
}

void Doctor::update(void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	auto screen = doctorData.screen;

	
	// Stop Movement When Key is Pressed
	if (!IsKeyDown(KEY_A))
	{
		player->rigidBody2D.translation.x += doctor_playerSpeed.x;
	}
	if (!IsKeyDown(KEY_W))
	{
		player->rigidBody2D.translation.y += doctor_playerSpeed.y;
	}


	// Clamp Player Inside Screen
	if (player->rigidBody2D.translation.x > screen.x + screen.width - player->getSize2D().x) doctor_playerSpeed.x = -10;
	if (player->rigidBody2D.translation.x < screen.x + player->getSize2D().x) doctor_playerSpeed.x = 10;

	if (player->rigidBody2D.translation.y > screen.y + screen.height - player->getSize2D().y) doctor_playerSpeed.y = -10;
	if (player->rigidBody2D.translation.y < screen.y + player->getSize2D().y) doctor_playerSpeed.y = 10;

	// Complete Game
	auto playerRect = Rectangle{player->getPosition2D().x, player->getPosition2D().y, player->getSize2D().x, player->getSize2D().y};
	if (CheckCollisionRecs(doctorData.goal, playerRect))
	{
		std::cout << "Scored \n";
		doctorData.score += 1;
	}

}
