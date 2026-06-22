#include "MiniGame_TimedSimonSays.h"

#include <SceneManager.h>

MiniGame* MiniGame_TimedSimonSays(Player* player)
{
	MiniGame* game = new MiniGame();
	game->name = "Simon Says (Timed)";
	game->update = &TimedSimonSays::update;
	game->draw = &TimedSimonSays::render;
	
	game->data = new MiniGameData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.3f, GetScreenHeight() * 0.3f));

	return game;
}

void TimedSimonSays::render(MiniGameData* data, void* player_ptr)
{
	auto player = static_cast<Player*>(player_ptr);
	
}

void TimedSimonSays::update(MiniGameData* data, void* player_ptr, float delta)
{
	auto player = static_cast<Player*>(player_ptr);
	


}
