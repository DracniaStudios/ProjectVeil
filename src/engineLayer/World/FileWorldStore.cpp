#include "FileWorldStore.h"

#include <algorithm>
#include <cassert>

namespace World {

	void FileWorldStore::BeginOpen(std::string_view worldKey)
	{
		std::lock_guard guard(lock);
		if (opened) { return; } // once per store
		opened = true;
		key = std::string(worldKey);
		state = {};
		status.store(WorldStoreStatus::Ready, std::memory_order_release);
	}

	bool FileWorldStore::ReadState(WorldState& out) const
	{
		const WorldStoreStatus now = Status();
		if (now != WorldStoreStatus::Ready && now != WorldStoreStatus::Degraded) { return false; }
		std::lock_guard guard(lock);
		out = state;
		return true;
	}

	void FileWorldStore::SubmitStationCompleted(std::uint64_t stationId)
	{
		const WorldStoreStatus now = Status();
		if (now == WorldStoreStatus::Closed) { return; } // after Close: ignored (W-11)
		assert(now == WorldStoreStatus::Ready && "Submit before the store is Ready (W-6)");

		std::lock_guard guard(lock);
		auto& done = state.completedStations;
		if (std::find(done.begin(), done.end(), stationId) == done.end()) { done.push_back(stationId); }
	}

	void FileWorldStore::SubmitFullWipe()
	{
		const WorldStoreStatus now = Status();
		if (now == WorldStoreStatus::Closed) { return; }
		assert(now == WorldStoreStatus::Ready && "Submit before the store is Ready (W-6)");

		std::lock_guard guard(lock);
		state.completedStations.clear();
	}

	void FileWorldStore::Close(std::chrono::milliseconds)
	{
		status.store(WorldStoreStatus::Closed, std::memory_order_release);
	}
}
