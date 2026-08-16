#include "MiniGame_FlappyBird.h"

#include <SceneManager.h>

namespace FlappyBirdSpace
{
	// Which side the player is currently being pushed toward — the game's only
	// state beyond the score, and it lives outside MiniGameData because
	// MiniGameData has nowhere to put it. Reset on construction (below).
	//
	// `static` (via the anonymous namespace) matters: these had external
	// linkage, so `leftGoal` and `rightGoal` in particular were global symbols
	// that any other translation unit could collide with at link time.
	bool isLeftGoalActive = false;
	bool isRightGoalActive = true;

	// The two scoring bands, in the same normalized space the obstacles use.
	// render() used to compute these into file scope as a side effect of
	// drawing, which update() then read — so on the first frame of every game
	// (update runs before the first draw) the goals were still {0,0,0,0} at the
	// screen origin, and any frame that updated without drawing tested against
	// stale rectangles. Deriving them lets both passes ask for the same answer.
	Rectangle GetLeftGoal(Rectangle screen) { return generateScaleRect(screen, Rectangle{ 1.05f, 1.05f, 0.1f, 0.95f }); }
	Rectangle GetRightGoal(Rectangle screen) { return generateScaleRect(screen, Rectangle{ 2.75f, 1.05f, 0.1f, 0.95f }); }

	// The play area, shared by update() and render() for the same reason
	Rectangle GetPlayScreen() { return getScreenScale({ 0.25f, 0.3f, 0.5f, 0.5f }); }
}

MiniGame* MiniGame_FlappyBird(Player* player)
{
	auto* game = new MiniGame();
	game->name = "Flappy Bird";
	game->update = &FlappyBird::update;
	game->draw = &FlappyBird::render;
	game->data = new MiniGameData;

	game->data->scoreGoal = 3;

	using namespace FlappyBirdSpace;
	// isLeftGoalActive/isRightGoalActive are file-scope, not part of MiniGameData,
	// so a retry/reset (which reconstructs the MiniGame via this function) must
	// reset them explicitly or the new game inherits whichever goal was active
	// when the previous attempt ended.
	isLeftGoalActive = false;
	isRightGoalActive = true;

	player->rigidBody2D.scale = Vector3(5.0f, 5.0f, 5.0f);
	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f));
	player->rigidBody2D.canMove = false;
	player->rigidBody2D.useGravity = false;

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

void FlappyBird::render(MiniGameData* data, Player* player)
{
	using namespace FlappyBirdSpace;
	/// Draw Background Screen
	Rectangle screen = GetPlayScreen();

	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 1, 1, 1 }), BLACK);
	DrawRectangleRec(generateScaleRect(screen, Rectangle{1.05f, 1.05f,0.95f, 0.95f}), DARKGREEN);

	/// Draw Goal Borders
	if (isLeftGoalActive) { DrawRectangleRec(GetLeftGoal(screen), GOLD); }
	if (isRightGoalActive) { DrawRectangleRec(GetRightGoal(screen), GOLD); }

	/// Draw Enemy Tiles

	for (auto &obj : data->obstacles)
	{
		Rectangle newSize =
		{
			1 + obj.x,
			1 + obj.y,
			obj.width,
			obj.height
		};

		// adjust object side accordingly

		DrawRectangleRec(generateScaleRect(screen, newSize), RED);
	}

	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 1, 1, 0.1f }), BLACK);
	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 2.5f, 1, 0.1f }), BLACK);

}

void FlappyBird::update(MiniGameData* data, Player* player, float deltaTime)
{
	auto scene = SceneManager::getInstance().currentScene;
	auto inputSystem = &InputSystem::getInstance();
	using namespace FlappyBirdSpace;

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

	Rectangle screen = GetPlayScreen();
	const Rectangle leftGoal = GetLeftGoal(screen);
	const Rectangle rightGoal = GetRightGoal(screen);

	const Vector2 playerPosition = player->rigidBody2D.getPosition();
	const float playerRadius = player->rigidBody2D.scale.x;

	// Game Logic
	// The active-goal test is part of the condition rather than an early `return`
	// out of update(). Returning skipped everything below it — the out-of-bounds
	// checks, the win check, gravity and the player's input — so simply resting
	// inside the goal that was already scored froze the whole game.
	if (isLeftGoalActive && CheckCollisionCircleRec(playerPosition, playerRadius, leftGoal))
	{
		std::cout << "Player Inside Left Goal \n";
		data->score++;
		isRightGoalActive = true;
		isLeftGoalActive = false;
		player->rigidBody2D.velocity = Vector2Negate(player->rigidBody2D.velocity);
		generateObstacle(2 + data->score);

	}
	else if (isRightGoalActive && CheckCollisionCircleRec(playerPosition, playerRadius, rightGoal))
	{
		std::cout << "Player Inside Right Goal \n";
		data->score++;
		isLeftGoalActive = true;
		isRightGoalActive = false;
		player->rigidBody2D.velocity = Vector2Negate(player->rigidBody2D.velocity);
		generateObstacle(2 + data->score);
	}

	if (player->rigidBody2D.getPosition().x < screen.x) scene->miniGame->data->isReset = true;
	if (player->rigidBody2D.getPosition().y < screen.y) scene->miniGame->data->isReset = true;
	if (player->rigidBody2D.getPosition().x > screen.x + screen.width) scene->miniGame->data->isReset = true;
	if (player->rigidBody2D.getPosition().y > screen.y + screen.height) scene->miniGame->data->isReset = true;

	// Win Condition
	CompleteMiniGame(data, player, BUFF_MOVEMENT);

	// Lose Condition
	 auto playerRect = Rectangle(player->rigidBody2D.translation.x, player->rigidBody2D.translation.y, player->rigidBody2D.scale.x, player->rigidBody2D.scale.y);

	if (!CheckCollisionRecs(playerRect, screen)); {
		scene->ReleaseMiniGame();
		std::cout << "[MiniGame/FlappyBird] Play Fail Sound\n";
	}

	// Player Logic
	{
		static int speed = 100;
		
		player->rigidBody2D.applyGravity();
		if (inputSystem->IsActionPressed(ACTION_MOVE_JUMP)) { player->rigidBody2D.jump(200); }

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

			if (CheckCollisionCircleRec(player->rigidBody2D.getPosition(), player->rigidBody2D.scale.x, obstacle)) scene->miniGame->data->isReset = true;
		}
	}
}
