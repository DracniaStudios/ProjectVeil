#include "MiniGame_FlappyBird.h"

#include <iostream>

namespace
{
	// The two scoring bands, in the same normalized space the obstacles use.
	// Derived rather than stored, so Update and DrawGame ask for the same answer.
	Rectangle GetLeftGoal(Rectangle screen) { return generateScaleRect(screen, Rectangle{ 1.05f, 1.05f, 0.1f, 0.95f }); }
	Rectangle GetRightGoal(Rectangle screen) { return generateScaleRect(screen, Rectangle{ 2.75f, 1.05f, 0.1f, 0.95f }); }

	// Inside the play screen, the area the bird must stay in
	Rectangle GetGreenArea(Rectangle screen) { return generateScaleRect(screen, Rectangle{ 1.05f, 1.05f, 0.95f, 0.95f }); }

	// Sideways push toward the live goal
	constexpr float kSpeed = 100.0f;
}

FlappyBird::FlappyBird(const MiniGameSetup& setup) : rng(setup.seed)
{
	name = "Flappy Bird";

	data.scoreGoal = 3;

	// The play area. Kept in the layout, so the MiniGame Inspector edits the real thing.
	data.screen = getScreenScale(setup.screenSize, { 0.25f, 0.3f, 0.5f, 0.5f });

	cursor.scale = Vector3(5.0f, 5.0f, 5.0f);
	cursor.teleport(Vector2(setup.screenSize.x * 0.5f, setup.screenSize.y * 0.5f));
	cursor.useGravity = false;
	cursor.bounds = Rectangle{ 0, 0, setup.screenSize.x, setup.screenSize.y };

	GenerateObstacles(4);
}

void FlappyBird::GenerateObstacles(int count)
{
	data.obstacles = {};

	// x 500 -> 1100

	for (int i = 0; i < count; i++)
	{
		// Gen Top Size
		Rectangle top = { getRandomFloat(rng, 0.2f, 1.5f), 0.05f, 0.05f, getRandomFloat(rng, 0.1f, 0.3f) };

		// Gen Bottom Size
		Rectangle bottom = { top.x, top.height + 0.9f, top.width, 0.5f - top.height };

		data.obstacles.push_back(top);
		data.obstacles.push_back(bottom);
	}
}

void FlappyBird::DrawGame() const
{
	/// Draw Background Screen
	const Rectangle screen = data.screen;

	DrawRectangleRec(generateScaleRect(screen, Rectangle{ 1, 1, 1, 1 }), BLACK);
	DrawRectangleRec(GetGreenArea(screen), DARKGREEN);

	/// Draw Goal Borders
	if (isLeftGoalActive) { DrawRectangleRec(GetLeftGoal(screen), GOLD); }
	if (isRightGoalActive) { DrawRectangleRec(GetRightGoal(screen), GOLD); }

	/// Draw Enemy Tiles

	for (auto &obj : data.obstacles)
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

MiniGameOutcome FlappyBird::Update(const MiniGameInput& input, float /*deltaTime*/)
{
	const Rectangle screen = data.screen;
	const Rectangle leftGoal = GetLeftGoal(screen);
	const Rectangle rightGoal = GetRightGoal(screen);

	const Vector2 cursorPosition = cursor.getPosition();
	const float cursorRadius = cursor.scale.x;

	// Game Logic
	// The active-goal test is part of the condition rather than an early return,
	// so resting inside the goal that was already scored doesn't freeze the game.
	if (isLeftGoalActive && CheckCollisionCircleRec(cursorPosition, cursorRadius, leftGoal))
	{
		std::cout << "Player Inside Left Goal \n";
		data.score++;
		isRightGoalActive = true;
		isLeftGoalActive = false;
		cursor.velocity = Vector2Negate(cursor.velocity);
		GenerateObstacles(2 + data.score);

	}
	else if (isRightGoalActive && CheckCollisionCircleRec(cursorPosition, cursorRadius, rightGoal))
	{
		std::cout << "Player Inside Right Goal \n";
		data.score++;
		isLeftGoalActive = true;
		isRightGoalActive = false;
		cursor.velocity = Vector2Negate(cursor.velocity);
		GenerateObstacles(2 + data.score);
	}

	if (!CheckCollisionRecs(cursor.getRectCentered(), GetGreenArea(screen))) { return MiniGameOutcome::Lost; }

	// Win Condition
	if (data.score >= data.scoreGoal) { return MiniGameOutcome::Won; }

	// Lose Condition. This one used to quit the station outright; a lost round now
	// always starts a fresh one, like every other way of losing.
	auto cursorRect = Rectangle(cursor.translation.x, cursor.translation.y, cursor.scale.x, cursor.scale.y);
	if (!CheckCollisionRecs(cursorRect, screen))
	{
		std::cout << "[MiniGame/FlappyBird] Play Fail Sound\n";
		return MiniGameOutcome::Lost;
	}

	// Cursor Logic
	{
		cursor.applyGravity(Vector2(0, 200));
		if (input.pressed.jump) { cursor.jump(200); }

		if (isLeftGoalActive) { cursor.applyForce(Vector2(-kSpeed, 0)); }
		if (isRightGoalActive) { cursor.applyForce(Vector2(kSpeed, 0)); }

	}

	// Entity Logic
	for (const Rectangle& obj : data.obstacles)
	{
		Rectangle newSize{
			1 + obj.x,
			1 + obj.y,
			obj.width,
			obj.height,
		};

		if (CheckCollisionCircleRec(cursor.getPosition(), cursor.scale.x, generateScaleRect(screen, newSize)))
		{
			return MiniGameOutcome::Lost;
		}
	}

	return MiniGameOutcome::Running;
}
