#pragma once
#ifndef SIM_SLOTS_H
#define SIM_SLOTS_H

// Slots: a player's place on the Host, numbered from 0 and given when they join
// (CONTEXT.md). The simulation knows players only by Slot, never as "local"
// (DedicatedServerPlan.md rule R2).

#include <cstdint>

namespace Sim {

	// The hard ceiling. Every array indexed by Slot is this long, and a World
	// save or config asking for more is refused (rule P-3).
	inline constexpr int kMaxPlayers = 16;

	// The in-process Host's cap on Dec 25 (DedicatedServerPlan Q22: defaults)
	inline constexpr int kDefaultMaxPlayers = 4;

	// Players' object ids sit in their own range, far above anything a GameMap
	// hands out, so a player id never collides with a world object's. Slot 0 is
	// the single-player player.
	inline constexpr std::uint64_t kFirstPlayerId = std::uint64_t{ 1 } << 60;

	inline constexpr bool IsValidSlot(int slot) { return slot >= 0 && slot < kMaxPlayers; }

	inline constexpr std::uint64_t PlayerIdForSlot(int slot)
	{
		return kFirstPlayerId + static_cast<std::uint64_t>(slot);
	}

	// The Slot a player id belongs to, or -1 for an id that is no player's
	inline constexpr int SlotForPlayerId(std::uint64_t id)
	{
		if (id < kFirstPlayerId || id >= kFirstPlayerId + kMaxPlayers) { return -1; }
		return static_cast<int>(id - kFirstPlayerId);
	}
}

#endif
