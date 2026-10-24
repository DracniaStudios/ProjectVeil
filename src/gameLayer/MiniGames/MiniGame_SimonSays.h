#pragma once
#ifndef SIMON_SAYS_H
#define SIMON_SAYS_H

#include <MiniGame.h>

// Press the shown sequence of directions, in order
class SimonSays : public MiniGame
{
public:
	explicit SimonSays(const MiniGameSetup& setup);

protected:
	MiniGameOutcome Update(const MiniGameInput& input, float deltaTime) override;
	void DrawGame() const override;

private:
	Vector2 screenSize = {};
};

#endif
