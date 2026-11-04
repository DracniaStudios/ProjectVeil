#pragma once
#ifndef SIM_STATION_TABLE_H
#define SIM_STATION_TABLE_H

// The Task stations of a World save, as the server owns them (MultiplayerSplitPlan.md
// G7; rules S-1, S-2, S-4, S-6, S-7; DedicatedServerPlan Step 2.5).
//
// S-1  Built once at load from the authored MiniGame Task stations, sorted by
//      id, on Host and client alike. It is the only truth for completion:
//      object flags are written from it and never read for decisions.
// S-2  Stations travel on the wire as their index in this table.
// S-6  A Target's state is derived, never stored:
//        enabled = authored XOR (odd number of stations completed in this
//                  world that point at it)
//      targetId 0 means the station itself. A station the World save already
//      marks completed is part of the authored level: its Target is authored
//      that way too, so it doesn't count toward the XOR.
// S-7  A station counts as in use whenever its holder isn't none.

#include <cstdint>
#include <vector>

struct GameMap;

namespace Sim {

	struct StationEntry
	{
		std::uint64_t id = 0;
		std::uint64_t targetId = 0;   // 0: the station is its own Target
		int game = 0;                 // the MiniGame it runs (its variation)
		float minimumSeconds = 3.0f;  // a Complete sooner than this after the grant is refused (S-3)

		bool authoredCompleted = false; // what the World save says, a starting value
		bool completedInWorld = false;  // completed through play: the World state
		int holder = -1;                // the Slot holding the claim, or -1
		std::uint32_t grantedTick = 0;  // the scene tick the claim was granted on

		bool Completed() const { return authoredCompleted || completedInWorld; }
		bool InUse() const { return holder >= 0; }
	};

	class StationTable
	{
	public:
		/**
		 * Builds the table from a map's MiniGame stations, sorted by id, and records
		 * every Target's authored enabled state. Stations already in the table keep
		 * their World state and holder. False, leaving the table empty, when the map
		 * has more stations than the protocol carries (kMaxStations, rule P-3).
		 */
		bool Build(const GameMap& map);

		int Count() const { return static_cast<int>(entries.size()); }
		const StationEntry& At(int index) const { return entries[static_cast<std::size_t>(index)]; }
		StationEntry& At(int index) { return entries[static_cast<std::size_t>(index)]; }
		bool IsValidIndex(int index) const { return index >= 0 && index < Count(); }

		// The index of a station id, or -1
		int IndexOf(std::uint64_t stationId) const;

		// The index of the station a Slot holds, or -1
		int HeldBy(int slot) const;

		// Marks the World state's completed stations (W-6). Ids not in the table are
		// returned in `unknown`, to be logged and dropped (S-6).
		void ApplyWorldState(const std::vector<std::uint64_t>& completed, std::vector<std::uint64_t>* unknown = nullptr);

		// The stations completed through play, in table order: the World state
		std::vector<std::uint64_t> CompletedInWorld() const;

		// Back to the World save as authored: nothing completed through play, no
		// holders (the Full wipe, C-2)
		void Wipe();

		// Writes the table onto the map's objects: each station's completed,
		// interactable and in-use flags, and every Target's derived enabled state
		// (S-6). Only objects whose state changes are touched.
		void WriteToMap(GameMap& map) const;

	private:
		struct TargetAuthored
		{
			std::uint64_t id = 0;
			bool enabled = true;
		};

		bool AuthoredEnabled(std::uint64_t targetId, bool fallback) const;

		std::vector<StationEntry> entries;
		std::vector<TargetAuthored> targets;
	};
}

#endif
