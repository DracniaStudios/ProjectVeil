#include "MiniGame_Doctor.h"

#include <SceneManager.h>


Vector2 doctor_playerSpeed = Vector2{ 10, 10 };

MiniGame* MiniGame_Doctor(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Doctor";
	game->update = &Doctor::update;
	game->draw = &Doctor::render;
	game->data = new MiniGameData;

	// Set Player Data
	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f));
	player->rigidBody2D.isEnabled = false;
	player->rigidBody2D.maxSpeed = 20;
	player->rigidBody2D.scale = Vector3(10, 10, 10);

	// Set Screen
	game->data->screen = getScreenScale({ 0.25f, 0.3f, 0.5f, 0.5f });

	// Create Goal Area
	auto rng = std::ranlux24_base(std::random_device{}());
	game->data->goal = getScreenScale(Rectangle{
		getRandomFloat(rng, 0.3f, 0.7f),
		getRandomFloat(rng, 0.35f, 0.75f),
		10,
		10
	});
	game->data->goal.width = 30;
	game->data->goal.height = 30;
	
	// Create Displayed Location on Sides
	Vector2 cubeSize = { 10, 10 };
	auto xCube = Rectangle{ game->data->screen.x + cubeSize.x, game->data->screen.y + cubeSize.y, cubeSize.x, cubeSize.y, };
	game->data->obstacles.push_back(xCube);
	auto yCube = Rectangle{ game->data->screen.x + cubeSize.x, game->data->screen.y + cubeSize.y, cubeSize.x, cubeSize.y, };
	game->data->obstacles.push_back(yCube);
	return game;
}

void Doctor::render(MiniGameData* data, void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	auto screen = data->screen;


	DrawRectangleRec(screen, BLACK);
	DrawRectangleRec(data->goal, GREEN);

	if (auto obj = &data->obstacles.front())
	{
		obj->x = player->getPosition2D().x - obj->width * 0.5f;
	}
	if (auto obj = &data->obstacles[1])
	{
		obj->y = player->getPosition2D().y - obj->height * 0.5f;
	}

	// 2 Pointer Cubes align to the Player's X and Y
	for (auto obj : data->obstacles)
	{
		DrawRectangleRec(obj, DARKBLUE);
	}
}

void Doctor::update(MiniGameData* data, void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	auto screen = data->screen;
	auto inputSystem = &InputSystem::getInstance();

	// Stop Movement When Key is Pressed
	if (!inputSystem->IsActionDown(ACTION_MOVE_LEFT))
	{
		player->rigidBody2D.translation.x += doctor_playerSpeed.x;
	}
	if (!inputSystem->IsActionDown(ACTION_MOVE_FORWARD))
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
	bool isMoving = !(inputSystem->IsActionDown(ACTION_MOVE_FORWARD) && inputSystem->IsActionDown(ACTION_MOVE_LEFT));

	if (CheckCollisionRecs(data->goal, playerRect) && isMoving == false)
	{
		std::cout << "Scored \n";
		player->applyHealthValue(3, false);
		data->isComplete = true;
	}

}
