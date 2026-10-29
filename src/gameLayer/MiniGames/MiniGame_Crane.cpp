#include "MiniGame_Crane.h"

namespace {
	// Horizontal drift per frame at 60 FPS
	constexpr int kSpeed = 2;
}

Crane::Crane(const MiniGameSetup& setup)
{
	name = "Crane";

	cursor.teleport(Vector2(setup.screenSize.x * 0.5f, setup.screenSize.y * 0.5f));
	cursor.scale = Vector3(5, 5, 5);
	cursor.bounds = Rectangle{ 0, 0, setup.screenSize.x, setup.screenSize.y };

	auto screen = data.screen = getScreenScale(setup.screenSize, { 0.25f, 0.2f, 0.5f, 0.7f });
	// Generate Obstacles
	{
		data.obstacles = {};
		auto rng = std::ranlux24_base(setup.seed);

		// Generate Obstacles
		for (int i = 0; i < 9; i++)
		{

			// Create Y Offset For Obstacles
			float width = getRandomFloat(rng, 0.1f, 0.4f);
			float height = setup.screenSize.y * 0.03f;
			float x = screen.x + (screen.x * getRandomFloat(rng, 0, 0.5f));
			float y = screen.y + (screen.height * (i * 0.1f));
			// Gen Top Size

			Rectangle left = {
				x,
				y,
				screen.width * width,
				height
			};

			// Gen Bottom Size
			// The right obstacle is anchored flush to the right wall below, so the
			// random x this used to compute was overwritten before it was ever read.
			width = screen.width * width;
			Rectangle right = {
				(screen.x + screen.width) - width,
				y,
				width,
				height
			};

			data.obstacles.push_back(left);
			data.obstacles.push_back(right);
		}
	}

	/// Goal Line
	data.goal = generateScaleRect(data.screen, { 1.0f, 1.0f, 1.0f, 0.1f });
	data.goal.y = data.screen.y + data.screen.height - data.goal.height;
}

void Crane::DrawGame() const
{
	/// Draw Background Screen
	DrawRectangleRec(data.screen, BLACK);
	DrawRectangleRec(data.goal, BLUE);

	for (auto& obj : data.obstacles)
	{
		DrawRectangleRec(obj, RED);
	}

}

MiniGameOutcome Crane::Update(const MiniGameInput& input, float deltaTime)
{
	/// Cursor Logic
	{
		cursor.applyGravity(Vector2{ 0, -150 });

		// kSpeed is a per-frame offset, so it is scaled by deltaTime and multiplied
		// back up by 60, keeping the drift off the refresh rate.
		const float step = kSpeed * 60.0f * deltaTime;

		if (input.held.jump) { cursor.jump(-200); }
		if (input.held.left) { cursor.translation += Vector3(-step, 0, 0); }
		if (input.held.right) { cursor.translation += Vector3(step, 0, 0); }

		if (cursor.getPosition().y < data.screen.y)
		{
			cursor.translation = Vector3(cursor.getPosition().x, data.screen.y + cursor.scale.x, 0);
		}

	}

	for (const Rectangle& obstacle : data.obstacles)
	{
		if (CheckCollisionCircleRec(cursor.getPosition(), cursor.scale.x, obstacle)) { return MiniGameOutcome::Lost; }
	}

	/// Goal Logic
	if (CheckCollisionCircleRec(cursor.getPosition(), cursor.scale.x, data.goal)) { return MiniGameOutcome::Won; }

	auto cursorRect = Rectangle(cursor.translation.x, cursor.translation.y, cursor.scale.x, cursor.scale.y);
	if (!CheckCollisionRecs(cursorRect, data.screen)) { return MiniGameOutcome::Lost; }

	return MiniGameOutcome::Running;
}
