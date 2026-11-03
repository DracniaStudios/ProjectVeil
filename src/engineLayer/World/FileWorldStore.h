#pragma once
#ifndef WORLD_FILE_WORLD_STORE_H
#define WORLD_FILE_WORLD_STORE_H

#include "WorldStore.h"

#include <atomic>
#include <mutex>
#include <string>

namespace World {

	/**
	 * Single-player's World store (MultiplayerSplitPlan.md Q37, Q47).
	 *
	 * Persists nothing new: World state lasts only for the session. A fresh
	 * session starts from the World save as authored, which single-player has
	 * always done, and the Game save (SaveSystem v3, written on exit) is left as
	 * it is until Step 2.7. Opening is immediate, so it never makes the game wait.
	 * Writes no file and opens no socket.
	 */
	class FileWorldStore final : public WorldStore
	{
	public:
		void BeginOpen(std::string_view worldKey) override;
		bool ReadState(WorldState& out) const override;
		void SubmitStationCompleted(std::uint64_t stationId) override;
		void SubmitFullWipe() override;
		WorldStoreStatus Status() const override { return status.load(std::memory_order_acquire); }
		std::size_t Pending() const override { return 0; }
		void Close(std::chrono::milliseconds flushDeadline) override;

		// The key BeginOpen was given, for the log
		const std::string& Key() const { return key; }

	private:
		std::atomic<WorldStoreStatus> status{ WorldStoreStatus::Closed };
		mutable std::mutex lock;
		WorldState state;
		std::string key;
		bool opened = false;
	};
}

#endif
