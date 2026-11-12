#pragma once
#ifndef CLIENT_ONLINE_WORLD_SYNC_H
#define CLIENT_ONLINE_WORLD_SYNC_H

// A joined client's copy of the Host's World state (MultiplayerSplitPlan.md
// §5.6; decisions Q37, Q47). The Host's WorldStore (SpacetimeDB) is the only
// durable record. A client keeps what it was told in a temporary file, which
// is deleted when the session ends and never read back as a save.
//
// "Requested Server": the client asks nothing extra. Every Snapshot carries all
// stations' completed flags, so the state of the server it joined is already
// in ClientWorld's Scene (rule P-8); SyncWorldFromServer writes that out.

#include <World/WorldStore.h>

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace Sim { class StationTable; }

namespace Client {

	class ClientWorld;

	// <temp dir>/veil_online/<world key>.json, the key made file-name safe
	std::filesystem::path OnlineWorldFile(std::string_view worldKey);

	/**
	 * Writes the World state the joined server last sent (the stations completed
	 * through play) to OnlineWorldFile(worldKey), via a sibling file renamed into
	 * place so a crash never leaves half a file. False, writing nothing, until the
	 * client is Welcomed and has received a Snapshot, or when the write fails.
	 * Cheap enough to call when the state changes; not for every frame.
	 */
	bool SyncWorldFromServer(const ClientWorld& client, const Sim::StationTable& stations,
		std::string_view worldKey, std::filesystem::path* written = nullptr);

	// Reads a file SyncWorldFromServer wrote. False for a missing, unreadable or
	// foreign file (wrong shape or another worldKey).
	bool ReadOnlineWorldState(const std::filesystem::path& file, std::string_view worldKey, World::WorldState& out);

	// Deletes the file; a missing one is fine
	void DiscardOnlineWorldState(std::string_view worldKey);
}

#endif
