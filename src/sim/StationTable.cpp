#include "StationTable.h"

#include <gameMap.h>
#include <net/Protocol.h>

#include <algorithm>

namespace Sim {

	bool StationTable::Build(const GameMap& map)
	{
		std::vector<StationEntry> built;
		std::vector<TargetAuthored> builtTargets;

		map.ForEachInteractable([&](const InteractableObject& station) {
			if (station.interactType != INTERACT_MINIGAME) { return; }
			StationEntry entry;
			entry.id = station.id;
			entry.targetId = station.targetId;
			entry.game = station.variation;
			entry.minimumSeconds = station.minimumSeconds;
			entry.authoredCompleted = station.isCompleted;
			built.push_back(entry);
		});
		std::sort(built.begin(), built.end(), [](const StationEntry& a, const StationEntry& b) { return a.id < b.id; });

		if (static_cast<int>(built.size()) > Net::kMaxStations)
		{
			entries.clear();
			targets.clear();
			return false;
		}

		// A station already known keeps what play has done to it
		for (StationEntry& entry : built)
		{
			const int previous = IndexOf(entry.id);
			if (previous < 0) { continue; }
			entry.completedInWorld = entries[static_cast<std::size_t>(previous)].completedInWorld;
			entry.holder = entries[static_cast<std::size_t>(previous)].holder;
			entry.grantedTick = entries[static_cast<std::size_t>(previous)].grantedTick;
		}

		// Every Target's state as authored, read now, while the map is as loaded
		for (const StationEntry& entry : built)
		{
			const std::uint64_t targetId = entry.targetId != 0 ? entry.targetId : entry.id;
			if (std::any_of(builtTargets.begin(), builtTargets.end(), [&](const TargetAuthored& t) { return t.id == targetId; })) { continue; }

			bool enabled = true;
			bool found = false;
			const auto read = [&](const GameObject& object) { if (object.id == targetId) { enabled = object.isEnabled; found = true; } };
			map.ForEachGameObject(read);
			map.ForEachInteractable(read);
			map.ForEachEntity(read);
			if (found) { builtTargets.push_back({ targetId, enabled }); }
		}

		entries = std::move(built);
		targets = std::move(builtTargets);
		return true;
	}

	int StationTable::IndexOf(std::uint64_t stationId) const
	{
		const auto it = std::lower_bound(entries.begin(), entries.end(), stationId,
			[](const StationEntry& entry, std::uint64_t id) { return entry.id < id; });
		if (it == entries.end() || it->id != stationId) { return -1; }
		return static_cast<int>(it - entries.begin());
	}

	int StationTable::HeldBy(int slot) const
	{
		if (slot < 0) { return -1; }
		for (int i = 0; i < Count(); ++i)
		{
			if (entries[static_cast<std::size_t>(i)].holder == slot) { return i; }
		}
		return -1;
	}

	void StationTable::ApplyWorldState(const std::vector<std::uint64_t>& completed, std::vector<std::uint64_t>* unknown)
	{
		for (const std::uint64_t id : completed)
		{
			const int index = IndexOf(id);
			if (index < 0)
			{
				if (unknown) { unknown->push_back(id); }
				continue;
			}
			// A station the World save already completes stays authored-completed
			StationEntry& entry = entries[static_cast<std::size_t>(index)];
			if (!entry.authoredCompleted) { entry.completedInWorld = true; }
		}
	}

	std::vector<std::uint64_t> StationTable::CompletedInWorld() const
	{
		std::vector<std::uint64_t> ids;
		for (const StationEntry& entry : entries) { if (entry.completedInWorld) { ids.push_back(entry.id); } }
		return ids;
	}

	void StationTable::Wipe()
	{
		for (StationEntry& entry : entries)
		{
			entry.completedInWorld = false;
			entry.holder = -1;
			entry.grantedTick = 0;
		}
	}

	bool StationTable::AuthoredEnabled(std::uint64_t targetId, bool fallback) const
	{
		for (const TargetAuthored& target : targets) { if (target.id == targetId) { return target.enabled; } }
		return fallback;
	}

	void StationTable::WriteToMap(GameMap& map) const
	{
		// Station flags, from the table only
		for (const StationEntry& entry : entries)
		{
			InteractableObject* station = map.FindInteractable(entry.id);
			if (station == nullptr) { continue; }
			station->isCompleted = entry.Completed();
			station->isInteractable = !entry.Completed();
			station->isRunningMiniGame = entry.InUse();
		}

		// Every Target: authored XOR (odd number of stations completed in play
		// pointing at it)
		for (const TargetAuthored& target : targets)
		{
			int toggles = 0;
			for (const StationEntry& entry : entries)
			{
				const std::uint64_t pointsAt = entry.targetId != 0 ? entry.targetId : entry.id;
				if (pointsAt == target.id && entry.completedInWorld) { ++toggles; }
			}
			const bool enabled = AuthoredEnabled(target.id, true) != ((toggles % 2) == 1);

			GameObject* object = map.FindWorldObject(target.id);
			if (object == nullptr || object->isEnabled == enabled) { continue; }
			// Toggling a Target loads or unloads its model: the logged R1 debt
			// (review S8), which moves with Step 1.4
			if (enabled) { object->onEnable(); }
			else { object->onDisable(); }
		}
	}
}
