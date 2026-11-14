#include "MiniGame.h"
#include <Render/Render.h>

MiniGameOutcome MiniGame::Tick(const MiniGameInput& input, float deltaTime)
{
	// The cursor moves first, the way Player::update2D used to run it ahead of
	// the game: forces a game applies this frame move the cursor next frame.
	cursor.update(deltaTime);
	return Update(input, deltaTime);
}

void MiniGame::Draw() const
{
	DrawGame();
	Render::Circle({ static_cast<float>(static_cast<int>(cursor.translation.x)), static_cast<float>(static_cast<int>(cursor.translation.y)) }, cursor.scale.x, WHITE);
}
