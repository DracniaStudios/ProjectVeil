#include "MiniGame_SimonSays.h"

#include <string>

// Step, Key
SimonSays::SimonSays(const MiniGameSetup& setup) : screenSize(setup.screenSize)
{
	name = "Simon says";

	data.scoreGoal = 25;

	cursor.teleport(Vector2(screenSize.x * 0.2f, screenSize.y * 0.2f));
	cursor.bounds = Rectangle{ 0, 0, screenSize.x, screenSize.y };

	auto rng = std::ranlux24_base(setup.seed);
	for (int i = 0; i < data.scoreGoal; ++i)
	{
		// Holds ID and Type. Update() only recognizes obstacle values 0-3 (one
		// per direction) — getRandomInt is inclusive of its upper bound, so this
		// must stay 3, not 4, or a generated obstacle could never be cleared.
		data.obstacles.push_back(Rectangle{static_cast<float>(i), static_cast<float>(getRandomInt(rng, 0, 3))});

	}
}


void SimonSays::DrawGame() const
{
	for (size_t i = 0; i < data.obstacles.size(); i++)
	{
		auto rect = getScreenScale(screenSize, Rectangle{ 0.025f * i, 0.45f, 0.01f, 0.01f });
		rect.x += 300;

		DrawRectangleRec(rect, data.score == static_cast<int>(i) ? LIGHTGRAY : BLUE);

		DrawText(std::to_string(static_cast<int>(data.obstacles[i].y)).c_str(), rect.x, rect.y, 20, WHITE);
		// Show Icon based On Selection
	}

}

MiniGameOutcome SimonSays::Update(const MiniGameInput& input, float /*deltaTime*/)
{
	// A single frame can register more than one key press; each branch re-checks the same
	// index, so chain them with else-if to avoid indexing obstacles[] once score reaches
	// scoreGoal (out-of-bounds) or awarding more than one point per frame.
	if (data.score < static_cast<int>(data.obstacles.size())) {
		if (input.pressed.forward && data.obstacles[data.score].y == 0) { data.score++; }
		else if (input.pressed.left && data.obstacles[data.score].y == 1) { data.score++; }
		else if (input.pressed.backward && data.obstacles[data.score].y == 2) { data.score++; }
		else if (input.pressed.right && data.obstacles[data.score].y == 3) { data.score++; }
	}

	return data.score >= data.scoreGoal ? MiniGameOutcome::Won : MiniGameOutcome::Running;
}
