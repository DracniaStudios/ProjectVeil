#pragma once
#ifndef FLAPPY_BIRD_H
#define FLAPPY_BIRD_H

#include <MiniGame.h>

// Fly back and forth between the two goals, through the obstacles, without leaving the green
class FlappyBird : public MiniGame
{
public:
	explicit FlappyBird(const MiniGameSetup& setup);

protected:
	MiniGameOutcome Update(const MiniGameInput& input, float deltaTime) override;
	void DrawGame() const override;

private:
	void GenerateObstacles(int count);

	// Which goal the bird is being pushed toward: the game's only state beyond its
	// layout. A fresh game is a fresh instance, so it always starts heading right.
	bool isLeftGoalActive = false;
	bool isRightGoalActive = true;

	std::ranlux24_base rng;
};

#endif
