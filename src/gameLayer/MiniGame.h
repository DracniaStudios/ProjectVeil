#pragma once
#ifndef MINIGAME_H
#define MINIGAME_H

#include <raylib.h>
#include <Physics.h>
#include <randomStuff.h>

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

/**
 * A game's layout and progress. The MiniGame Inspector shows and edits it live,
 * and the session's history keeps copies of it.
 */
struct MiniGameData
{
	int score = 0;
	int scoreGoal = 1;

	std::vector<Rectangle> obstacles = {};
	Rectangle screen = {};
	Rectangle goal = {};
};

// How a round of a MiniGame went this frame. See CONTEXT.md.
enum class MiniGameOutcome
{
	Running,
	Won,
	Lost, // The round is over; the session starts a fresh one
};

// The actions a MiniGame reads
struct MiniGameKeys
{
	bool left = false;
	bool right = false;
	bool forward = false;
	bool backward = false;
	bool jump = false;
	bool sprint = false;
};

// One frame of the player's input, read by the scene and handed to the game
struct MiniGameInput
{
	MiniGameKeys held = {};    // Down this frame
	MiniGameKeys pressed = {}; // Went down this frame
	Vector2 screenSize = {};   // The window, for a round that starts this frame
};

// What a game is built from. Everything it lays out or randomises comes from here.
struct MiniGameSetup
{
	Vector2 screenSize = {};
	std::uint32_t seed = 0;
};

/**
 * A short 2D game played inside the MiniGame overlay. See CONTEXT.md.
 *
 * Owns all of its state, its layout and the player's cursor, so a fresh round is
 * a fresh instance. It never ends itself: Tick says how the round went and the
 * MiniGameSession acts on it, after Tick has returned.
 */
class MiniGame
{
public:
	virtual ~MiniGame() = default;

	// One frame: the cursor's physics, then the game's rules
	MiniGameOutcome Tick(const MiniGameInput& input, float deltaTime);

	// The game, then the player's cursor over it
	void Draw() const;

	const char* name = "MiniGame";
	MiniGameData data = {};             // Layout and progress
	Physics2D::RigidBody2D cursor = {}; // The player, inside this game

protected:
	virtual MiniGameOutcome Update(const MiniGameInput& input, float deltaTime) = 0;
	virtual void DrawGame() const = 0;
};

/// Mini Game IDs
#define MINI_GAME_FLAPPY_BIRD_ID 0
#define MINI_GAME_CRANE_ID 1
#define MINI_GAME_DOCTOR_ID 2
#define MINI_GAME_SIMON_SAYS_ID 3
#define MINI_GAME_MAZE_ID 4
#define MINI_GAME_RO_SHAM_BOO_ID 5 // Rock Paper Scissors

<<<<<<< HEAD
// Builds a fresh game by id, or nullptr for an id that names no game
std::unique_ptr<MiniGame> CreateMiniGame(int id, const MiniGameSetup& setup);

=======
>>>>>>> origin/claude-created
inline const char* miniGameIdToString(int id)
{
	switch (id)
	{
	case MINI_GAME_FLAPPY_BIRD_ID: return "Flappy Bird";
	case MINI_GAME_CRANE_ID:       return "Crane";
	case MINI_GAME_DOCTOR_ID:      return "Doctor";
	case MINI_GAME_SIMON_SAYS_ID:  return "Simon Says";
	case MINI_GAME_MAZE_ID:        return "Maze";
	case MINI_GAME_RO_SHAM_BOO_ID: return "Ro Sham Boo";
	default:                       return "Unknown";
	}
}

/// Convert and store any objects as pixels before rendering
/*** Draw MiniGame ***/
inline Rectangle generateScaleRect(Rectangle screen, Rectangle rect)
{
	Rectangle r = {
		screen.x * rect.x,
		screen.y * rect.y,
		screen.width * rect.width,
		screen.height * rect.height
	};
	return r;
}

inline Rectangle generateObstacleRect(Rectangle screen, Rectangle rect)
{
	Rectangle newSize{};
	newSize.x = 1 + rect.x;
	newSize.y = 1 + rect.y;
	newSize.width = rect.width;
	newSize.height = rect.height;

	return generateScaleRect(screen, newSize);

}

// A rectangle given as fractions of the screen, in pixels
inline Rectangle getScreenScale(Vector2 screenSize, Rectangle rect)
{
	Rectangle r{};
	r.x = screenSize.x * rect.x;
	r.y = screenSize.y * rect.y;
	r.width = screenSize.x * rect.width;
	r.height = screenSize.y * rect.height;

	return r;
}

#endif
