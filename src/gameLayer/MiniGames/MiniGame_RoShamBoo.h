#pragma once
#ifndef MINI_GAME_RO_SHAM_BOO_H
#define MINI_GAME_RO_SHAM_BOO_H

#include <MiniGame.h>

// Rock Paper Scissors: pick the one card of three the game has in mind
class RoShamBoo : public MiniGame
{
public:
	explicit RoShamBoo(const MiniGameSetup& setup);

protected:
	MiniGameOutcome Update(const MiniGameInput& input, float deltaTime) override;
	void DrawGame() const override;

private:
	Vector2 screenSize = {};
	MiniGameKeys held = {}; // This frame's held keys, so the card being chosen lights up
};

#endif
