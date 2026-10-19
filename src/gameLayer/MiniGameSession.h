#pragma once
#ifndef MINIGAMESESSION_H
#define MINIGAMESESSION_H

#include <MiniGame.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <random>

/**
 * Two-deep history of MiniGame progress, kept by value.
 *
 * A game's layout lives exactly as long as the game, so walking away from a
 * half-finished station would throw the progress away with no record it ever
 * existed. These snapshots outlive it. `current` tracks the live game's last
 * known state; `previous` is the one it displaced.
 *
 * A default MiniGameData is indistinguishable from a genuine score of zero, so
 * the flags tell "played and scored nothing" from "never played".
 */
struct MiniGameHistory
{
	MiniGameData current = {};
	bool hasCurrent = false;

	MiniGameData previous = {};
	bool hasPrevious = false;
};

// How the session's game went this frame, and which game and station it was
struct MiniGameResult
{
	MiniGameOutcome outcome = MiniGameOutcome::Running;
	int gameId = -1;
	std::uint64_t stationId = 0; // 0 for a Practice run
};

/**
 * Runs one MiniGame at a time for a scene.
 *
 * Owns the running game and acts on how its rounds go: a Lost round is replaced
 * by a fresh game of the same kind, and a Won one ends the game and is reported
 * to the caller, who applies the win to the world. Nothing here touches the
 * world, which is why it can be tested without one.
 */
class MiniGameSession
{
public:
	// Builds a fresh game by id, or nullptr for an id that names no game
	using Factory = std::function<std::unique_ptr<MiniGame>(int gameId, const MiniGameSetup& setup)>;

	explicit MiniGameSession(Factory factory);

	/** Commands **/
	// Starts a game for a station (0 for a Practice run), replacing any running
	// one. False, with nothing changed, for an id that names no game.
	bool Start(int gameId, std::uint64_t stationId, Vector2 screenSize, std::uint32_t seed);

	// Runs one frame of the game. Does nothing, and reports Running, with no game.
	MiniGameResult Tick(const MiniGameInput& input, float deltaTime);

	// Ends the running game unwon, keeping how far the player got
	void Quit();

	// The World Editor's buttons: a fresh round as if this one were lost, and an
	// end as if it were won, reported the way a real win is
	void Retry(Vector2 screenSize);
	MiniGameResult Win();

	// The running game and the player's cursor in it
	void Draw() const;

	/** Queries **/
	bool IsRunning() const { return game != nullptr; }

	// The game running, or the last one that ran; -1 before any has
	int GameId() const { return gameId; }

	// The station the running game was started from; 0 for a Practice run or none
	std::uint64_t StationId() const { return stationId; }

	// The running game, for the MiniGame Inspector to show and edit; nullptr for none
	MiniGame* Game() const { return game.get(); }

	const MiniGameHistory& History() const { return history; }

private:
	// Makes `built` the running game, rolling the history back one slot first
	void Replace(std::unique_ptr<MiniGame> built);

	// Frees the running game, keeping its final state in the history
	void End();

	// Replaces the running game with a fresh one of the same kind
	void Restart(Vector2 screenSize);

	// Copies the running game's progress into the history; nothing without a game
	void Snapshot();

	Factory factory;
	std::unique_ptr<MiniGame> game;
	int gameId = -1;
	std::uint64_t stationId = 0;
	MiniGameHistory history = {};

	// Seeds every round this session builds, from the seed Start was given, so a
	// fixed seed replays the same rounds and each retry is still randomised afresh
	std::mt19937 seeds;
};

#endif // MINIGAMESESSION_H
