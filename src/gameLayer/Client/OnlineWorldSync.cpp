#include "OnlineWorldSync.h"

#include <Client/ClientWorld.h>
#include <sim/StationTable.h>

#include <nlohmann/json.hpp>

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace Client {

	namespace {
		constexpr int kFormat = 1;

		std::string FileSafe(std::string_view key)
		{
			std::string out;
			for (const char c : key)
			{
				out += (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ? c : '_';
			}
			return out;
		}
	}

	std::filesystem::path OnlineWorldFile(std::string_view worldKey)
	{
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
		if (ec) { dir = "."; }
		return dir / "veil_online" / (FileSafe(worldKey) + ".json");
	}

	bool SyncWorldFromServer(const ClientWorld& client, const Sim::StationTable& stations,
		std::string_view worldKey, std::filesystem::path* written)
	{
		// Welcomed and at least one Snapshot applied: before that the table is
		// the World save as authored, not the server's state
		if (client.State() != ConnectionState::Welcomed || client.NewestServerTick() == 0) { return false; }

		const std::filesystem::path file = OnlineWorldFile(worldKey);
		std::error_code ec;
		std::filesystem::create_directories(file.parent_path(), ec);
		if (ec) { std::cerr << "[OnlineWorld] can't create " << file.parent_path() << ": " << ec.message() << "\n"; return false; }

		nlohmann::json doc;
		doc["format"] = kFormat;
		doc["worldKey"] = std::string(worldKey);
		doc["serverTick"] = client.NewestServerTick();
		doc["completedStations"] = stations.CompletedInWorld();

		const std::filesystem::path partial = file.string() + ".tmp";
		{
			std::ofstream out(partial, std::ios::binary | std::ios::trunc);
			if (!out) { std::cerr << "[OnlineWorld] can't write " << partial << "\n"; return false; }
			out << doc.dump();
			if (!out.flush()) { std::cerr << "[OnlineWorld] write failed: " << partial << "\n"; return false; }
		}
		std::filesystem::rename(partial, file, ec); // replaces an existing file on Windows and POSIX
		if (ec)
		{
			std::cerr << "[OnlineWorld] can't replace " << file << ": " << ec.message() << "\n";
			std::filesystem::remove(partial, ec);
			return false;
		}
		if (written) { *written = file; }
		return true;
	}

	bool ReadOnlineWorldState(const std::filesystem::path& file, std::string_view worldKey, World::WorldState& out)
	{
		std::ifstream in(file, std::ios::binary);
		if (!in) { return false; }
		const nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
		if (doc.is_discarded() || !doc.is_object()) { return false; }
		if (doc.value("format", 0) != kFormat || doc.value("worldKey", std::string()) != worldKey) { return false; }

		const auto found = doc.find("completedStations");
		if (found == doc.end() || !found->is_array()) { return false; }

		World::WorldState state;
		for (const nlohmann::json& id : *found)
		{
			if (!id.is_number_unsigned()) { return false; }
			state.completedStations.push_back(id.get<std::uint64_t>());
		}
		out = std::move(state);
		return true;
	}

	void DiscardOnlineWorldState(std::string_view worldKey)
	{
		std::error_code ec;
		std::filesystem::remove(OnlineWorldFile(worldKey), ec);
	}
}
