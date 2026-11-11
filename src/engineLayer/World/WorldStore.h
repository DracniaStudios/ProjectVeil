#pragma once

// World data interface (MultiplayerSplitPlan.md §5.2, decisions Q37, Q46, Q60).
// Two backends: FileWorldStore (engine layer, single-player) and
// SpacetimeWorldStore (src/net/spacetime/, the host). Rule R8: no SpacetimeDB or
// HTTP-library type appears in this header or anywhere outside src/net/spacetime/.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace World
{
	enum class WorldStoreStatus { Opening, Ready, Degraded, Failed, Closed };

	// World state: which Task stations are completed. Targets are derived, never
	// stored: enabled = authored XOR (odd number of completed stations pointing at it).
	struct WorldState
	{
		std::vector<std::uint64_t> completedStations;
	};

	class WorldStore
	{
	public:
		virtual ~WorldStore() = default;

		// Once per store. Returns at once; Status() reports progress (W-5).
		// worldKey = world_id + content hash, so an edited World save opens a fresh world.
		virtual void BeginOpen(std::string_view worldKey) = 0;

		// False unless Status() is Ready or Degraded (W-6).
		virtual bool ReadState(WorldState& out) const = 0;

		// Called from the sim tick. Append only: short lock, no I/O, no logging (W-1).
		virtual void SubmitStationCompleted(std::uint64_t stationId) = 0;
		virtual void SubmitFullWipe() = 0;

		virtual WorldStoreStatus Status() const = 0; // atomic, acquire
		virtual std::size_t Pending() const = 0;     // atomic

		// Idempotent. Flushes until the deadline, then stops the request in flight (W-7).
		virtual void Close(std::chrono::milliseconds flushDeadline) = 0;
	};
}
