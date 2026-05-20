#include "Crane.h"

#include <Player.h>
#include <SceneManager.h>


void Crane::render(void* manager_ptr, void* player_ptr)
{
	std::ranlux24_base rng(std::random_device{}());

	Rectangle screen = generateScaleRect(
		Rectangle{ 0, 0, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) },
			Rectangle{0.2f, 0.3f, 0.5f, 0.5f} 
	);

	Rectangle goal = generateScaleRect(screen, 
		Rectangle{ 1, 0.9f, 1, 0.1f }
	);


}

void Crane::update(void* manager_ptr, void* player_ptr, float deltaTime)
{
	
}

MiniGame* MiniGame_crane()
{
	MiniGame* game = new MiniGame();
	game->name = "Crane";
	game->update = &Crane::update;
	game->draw = &Crane::render;
	return game;
}