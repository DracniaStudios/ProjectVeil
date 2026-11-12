#pragma once
#ifndef SIM_GAME_EVENTS_H
#define SIM_GAME_EVENTS_H

// Facts the simulation reports to the clients, collected during a tick in the
// Scene's outbox. The server sends each one reliably (MultiplayerSplitPlan.md
// §5.3: PlayerCaught, RoundOver, StationEvent); in single-player the same events
// reach the same client code through Loopback.

#include <sim/Rules.h>

#include <cstdint>

namespace Sim {

	enum class GameEventType : std::uint8_t {
		PlayerCaught,  // slot
		RoundOver,     // outcome, roundEpoch (the epoch the round ended in)
		StationEvent,  // stationEvent, stationIndex, slot
	};

	// What happened at a Task station (§5.3 StationEvent)
	enum class StationEventKind : std::uint8_t { Granted, Denied, Released, Completed, Count };

	// What a client asks of a Task station (§5.3 StationRequest). The Slot is the
	// connection's, never part of the request (P-5).
	enum class StationAction : std::uint8_t { Claim, Release, Complete, Count };

	struct StationRequest {
		StationAction action = StationAction::Claim;
		int stationIndex = -1;
	};

	struct GameEvent {
		GameEventType    type         = GameEventType::PlayerCaught;
		int              slot         = -1;
		RoundOutcome     outcome      = RoundOutcome::Ongoing;
		std::uint32_t    roundEpoch   = 0;
		StationEventKind stationEvent = StationEventKind::Granted;
		int              stationIndex = -1;
	};
}

#endif
