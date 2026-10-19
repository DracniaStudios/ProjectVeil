#include <MiniGame.h>

#include "MiniGame_Crane.h"
#include "MiniGame_Doctor.h"
#include "MiniGame_FlappyBird.h"
#include "MiniGame_Maze.h"
#include "MiniGame_RoShamBoo.h"
#include "MiniGame_SimonSays.h"

std::unique_ptr<MiniGame> CreateMiniGame(int id, const MiniGameSetup& setup)
{
	switch (id)
	{
	case MINI_GAME_FLAPPY_BIRD_ID: return std::make_unique<FlappyBird>(setup);
	case MINI_GAME_CRANE_ID:       return std::make_unique<Crane>(setup);
	case MINI_GAME_DOCTOR_ID:      return std::make_unique<Doctor>(setup);
	case MINI_GAME_SIMON_SAYS_ID:  return std::make_unique<SimonSays>(setup);
	case MINI_GAME_MAZE_ID:        return std::make_unique<Maze>(setup);
	case MINI_GAME_RO_SHAM_BOO_ID: return std::make_unique<RoShamBoo>(setup);
	default:                       return nullptr;
	}
}
