#pragma once
#ifndef MAZE_H
#define MAZE_H

#include <MiniGame.h>

// Unfinished: draws its grid and lets the cursor wander, but can be neither won nor lost
class Maze : public MiniGame
{
public:
	explicit Maze(const MiniGameSetup& setup);

protected:
	MiniGameOutcome Update(const MiniGameInput& input, float deltaTime) override;
	void DrawGame() const override;
};

#endif
