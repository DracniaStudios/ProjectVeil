#pragma once
#ifndef SIM_RULES_H
#define SIM_RULES_H

// What a catch or an escape means for the round (DedicatedServerPlan.md Step
// 2.6; MultiplayerSplitPlan.md Q41, Q61, rules C-1 and C-3). The server records
// facts ("Slot 2 caught", "Slot 0 escaped"); this one function turns them into
// an outcome, so the multiplayer rule can change without touching the stalker.

#include <cstdint>

namespace Sim {

	enum class RoundOutcome : std::uint8_t { Ongoing, Caught, Escaped, Count };

	struct RoundState {
		int  playersInRound  = 0; // Slots with a player
		int  playersCaught   = 0; // Slots the stalker caught this tick
		int  playersEscaped  = 0; // Slots through the unlocked exit this tick
	};

	// Veil's Dec 25 rule, the same for one player or four: any catch ends the
	// round for everyone with a Full wipe (Q41), and any escape through the
	// unlocked exit ends it for everyone with the end card (Q61). A catch and an
	// escape in the same tick count as the catch. The multiplayer rule (out,
	// downed and revivable, team loses) is decided at the multiplayer update and
	// replaces this body only.
	inline RoundOutcome EvaluateRound(const RoundState& state)
	{
		if (state.playersCaught > 0)  { return RoundOutcome::Caught; }
		if (state.playersEscaped > 0) { return RoundOutcome::Escaped; }
		return RoundOutcome::Ongoing;
	}

	inline const char* RoundOutcomeName(RoundOutcome outcome)
	{
		switch (outcome)
		{
		case RoundOutcome::Ongoing: return "Ongoing";
		case RoundOutcome::Caught:  return "Caught";
		case RoundOutcome::Escaped: return "Escaped";
		default:                    return "Unknown";
		}
	}
}

#endif
