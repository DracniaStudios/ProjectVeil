#include "MiniGame_Doctor.h"

Doctor::Doctor(const MiniGameSetup& setup)
{
	name = "Doctor";

	// Set Cursor Data. Doctor moves the cursor itself, so its physics stay off.
	cursor.teleport(Vector2(setup.screenSize.x * 0.5f, setup.screenSize.y * 0.5f));
	cursor.isEnabled = false;
	cursor.maxSpeed = 20;
	cursor.scale = Vector3(10, 10, 10);

	// Set Screen
	data.screen = getScreenScale(setup.screenSize, { 0.25f, 0.3f, 0.5f, 0.5f });

	// Create Goal Area
	auto rng = std::ranlux24_base(setup.seed);
	data.goal = getScreenScale(setup.screenSize, Rectangle{
		getRandomFloat(rng, 0.3f, 0.7f),
		getRandomFloat(rng, 0.35f, 0.75f),
		10,
		10
	});
	data.goal.width = 30;
	data.goal.height = 30;

	// Create Displayed Location on Sides
	Vector2 cubeSize = { 10, 10 };
	auto xCube = Rectangle{ data.screen.x + cubeSize.x, data.screen.y + cubeSize.y, cubeSize.x, cubeSize.y, };
	data.obstacles.push_back(xCube);
	auto yCube = Rectangle{ data.screen.x + cubeSize.x, data.screen.y + cubeSize.y, cubeSize.x, cubeSize.y, };
	data.obstacles.push_back(yCube);
}

void Doctor::DrawGame() const
{
	DrawRectangleRec(data.screen, BLACK);
	DrawRectangleRec(data.goal, GREEN);

	// 2 Pointer Cubes align to the cursor's X and Y
	for (auto obj : data.obstacles)
	{
		DrawRectangleRec(obj, DARKBLUE);
	}
}

MiniGameOutcome Doctor::Update(const MiniGameInput& input, float delta)
{
	const Rectangle screen = data.screen;
	const Vector2 position = { cursor.translation.x, cursor.translation.y };
	const Vector2 size = { cursor.scale.x, cursor.scale.y };

	// driftSpeed is a per-frame drift amount (its sign flips at the screen edges
	// below), so it is scaled by deltaTime and multiplied back up by 60, keeping
	// the "hold a direction to stop drifting" difficulty off the refresh rate.
	const float driftScale = 60.0f * delta;

	// Stop Movement When Key is Pressed
	if (!input.held.left)
	{
		cursor.translation.x += driftSpeed.x * driftScale;
	}
	if (!input.held.forward)
	{
		cursor.translation.y += driftSpeed.y * driftScale;
	}

	// Clamp Cursor Inside Screen
	if (cursor.translation.x > screen.x + screen.width - size.x) driftSpeed.x = -10;
	if (cursor.translation.x < screen.x + size.x) driftSpeed.x = 10;

	if (cursor.translation.y > screen.y + screen.height - size.y) driftSpeed.y = -10;
	if (cursor.translation.y < screen.y + size.y) driftSpeed.y = 10;

	// The pointer cubes follow the cursor along the sides. Moved here from drawing,
	// which is const: drawing no longer changes the game.
	if (data.obstacles.size() >= 2)
	{
		data.obstacles[0].x = cursor.translation.x - data.obstacles[0].width * 0.5f;
		data.obstacles[1].y = cursor.translation.y - data.obstacles[1].height * 0.5f;
	}

	// Complete Game
	auto playerRect = Rectangle{ position.x, position.y, size.x, size.y };
	bool isMoving = !(input.held.forward && input.held.left);

	if (CheckCollisionRecs(data.goal, playerRect) && isMoving == false)
	{
		return MiniGameOutcome::Won;
	}

	return MiniGameOutcome::Running;
}
