#pragma once
#ifndef CRANE_H
#define CRANE_H

#include <MiniGame.h>

// Lower the crane through a gap in each row of obstacles, down to the goal line
class Crane : public MiniGame
{
public:
	explicit Crane(const MiniGameSetup& setup);

protected:
	MiniGameOutcome Update(const MiniGameInput& input, float deltaTime) override;
	void DrawGame() const override;
};

#endif
