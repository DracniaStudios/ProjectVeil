#include "MiniGame_RoShamBoo.h"

#include <iostream>

RoShamBoo::RoShamBoo(const MiniGameSetup& setup) : screenSize(setup.screenSize)
{
	name = "RoShamBoo";

	cursor.teleport(Vector2(screenSize.x * 0.4f, screenSize.y * 0.4f));
	cursor.bounds = Rectangle{ 0, 0, screenSize.x, screenSize.y };

	auto rng = std::ranlux24_base(setup.seed);
	data.scoreGoal = getRandomInt(rng, 1, 3);
	std::cout << name << " Goal: " << data.scoreGoal << "\n";
}

void RoShamBoo::DrawGame() const
{
	// Display 3 Cards
	auto leftCard = getScreenScale(screenSize, Rectangle{ 0.1f, 0.3f, 0.2f, 0.5f });
	auto middleCard = getScreenScale(screenSize, Rectangle{ 0.4f, 0.3f, 0.2f, 0.5f });
	auto rightCard = getScreenScale(screenSize, Rectangle{ 0.7f, 0.3f, 0.2f, 0.5f });

	// Highlighted from the same actions Update() selects on
	DrawRectangleRec(leftCard, held.left ? YELLOW : RED);
	DrawRectangleRec(middleCard, held.backward ? YELLOW : WHITE);
	DrawRectangleRec(rightCard, held.right ? YELLOW : BLUE);

}

MiniGameOutcome RoShamBoo::Update(const MiniGameInput& input, float /*deltaTime*/)
{
	held = input.held;

	// Select Left
	if (input.pressed.left) { data.score = 1; }
	// Select Middle
	if (input.pressed.backward) { data.score = 2; }
	// Select Right
	if (input.pressed.right) { data.score = 3; }

	return data.score == data.scoreGoal ? MiniGameOutcome::Won : MiniGameOutcome::Running;
}
