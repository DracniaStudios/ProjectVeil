#include "MiniGameSession.h"

#include <utility>

MiniGameSession::MiniGameSession(Factory factory) : factory(std::move(factory))
{
}

bool MiniGameSession::Start(int newGameId, std::uint64_t newStationId, Vector2 screenSize, std::uint32_t seed)
{
	// Tried on a copy of the seed sequence, so a rejected id leaves it untouched
	std::mt19937 trialSeeds(seed);
	std::unique_ptr<MiniGame> built = factory(newGameId, MiniGameSetup{ screenSize, trialSeeds() });
	if (built == nullptr) { return false; }

	seeds = trialSeeds;
	gameId = newGameId;
	stationId = newStationId;
	Replace(std::move(built));
	return true;
}

MiniGameResult MiniGameSession::Tick(const MiniGameInput& input, float deltaTime)
{
	MiniGameResult result{ MiniGameOutcome::Running, gameId, stationId };
	if (game == nullptr) { return result; }

	result.outcome = game->Tick(input, deltaTime);

	// The game has returned, so replacing or ending it here frees nothing still running
	if (result.outcome == MiniGameOutcome::Lost) { Restart(input.screenSize); }
	else if (result.outcome == MiniGameOutcome::Won) { End(); }

	return result;
}

void MiniGameSession::Quit()
{
	End();
}

void MiniGameSession::Retry(Vector2 screenSize)
{
	if (game == nullptr) { return; }
	Restart(screenSize);
}

MiniGameResult MiniGameSession::Win()
{
	MiniGameResult result{ MiniGameOutcome::Running, gameId, stationId };
	if (game == nullptr) { return result; }

	result.outcome = MiniGameOutcome::Won;
	End();
	return result;
}

void MiniGameSession::Draw() const
{
	if (game != nullptr) { game->Draw(); }
}

void MiniGameSession::Restart(Vector2 screenSize)
{
	if (std::unique_ptr<MiniGame> fresh = factory(gameId, MiniGameSetup{ screenSize, seeds() }))
	{
		Replace(std::move(fresh));
	}
}

void MiniGameSession::End()
{
	// Captured once more on the way out, so the history holds the final state
	// rather than a stale mid-run reading
	Snapshot();
	game.reset();
	stationId = 0;
}

void MiniGameSession::Replace(std::unique_ptr<MiniGame> built)
{
	// Snapshot first: what the outgoing game holds right now, the score the player
	// actually reached, is the whole point of keeping it
	Snapshot();
	history.previous = history.current;
	history.hasPrevious = history.hasCurrent;

	game = std::move(built);

	// The incoming game's opening state
	history.current = game->data;
	history.hasCurrent = true;
}

void MiniGameSession::Snapshot()
{
	if (game == nullptr) { return; }

	history.current = game->data;
	history.hasCurrent = true;
}
