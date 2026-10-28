#pragma once
#ifndef DOCTOR_H
#define DOCTOR_H

#include <MiniGame.h>

// The cursor drifts on its own; hold both directions to stop it on the goal
class Doctor : public MiniGame
{
public:
	explicit Doctor(const MiniGameSetup& setup);

protected:
	MiniGameOutcome Update(const MiniGameInput& input, float deltaTime) override;
	void DrawGame() const override;

private:
	// The drift per frame at 60 FPS. Its signs flip at the screen edges, so a
	// fresh game starts from this again rather than from wherever the last ended.
	Vector2 driftSpeed = Vector2{ 10, 10 };
};

#endif
