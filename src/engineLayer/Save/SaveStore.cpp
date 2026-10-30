#include "SaveStore.h"

#include <fstream>
#include <iostream>

namespace SaveSystem
{
	namespace fs = std::filesystem;

	fs::path FileSaveStore::PathOf(const std::string& name) const
	{
		return directory / (name + ".json");
	}

	bool FileSaveStore::Write(const std::string& name, const Json& save)
	{
		const fs::path path = PathOf(name);
		const fs::path tmpPath = path.string() + ".tmp";
		const fs::path bakPath = path.string() + ".bak";

		std::error_code errorCode;
		if (path.has_parent_path())
		{
			fs::create_directories(path.parent_path(), errorCode);
		}

		std::ofstream file(tmpPath, std::ios::binary);
		if (!file.is_open()) { return false; }

		file << save.dump(2);
		file.close();

		// Backing up the previous save is best-effort (it may not exist yet); only the
		// final rename actually delivers the new save, so its failure must be reported.
		fs::rename(path, bakPath, errorCode);
		errorCode.clear();
		fs::rename(tmpPath, path, errorCode);
		if (errorCode)
		{
			std::cerr << "[Save System] Failed to finalize save file: " << path.string() << " (" << errorCode.message() << ")\n";
			return false;
		}
		return true;
	}

	bool FileSaveStore::Read(const std::string& name, Json& save) const
	{
		const fs::path path = PathOf(name);
		std::ifstream file(path, std::ios::binary);

		if (!file.is_open())
		{
			std::cerr << "[Save System] Failed to open save file: " << path.string() << "\n";
			return false;
		}

		save = Json::parse(file, nullptr, false);
		file.close();

		if (save.is_discarded())
		{
			std::cerr << "[Save System] Save file is corrupted: " << path.string() << "\n";
			return false;
		}

		return true;
	}

	bool MemorySaveStore::Write(const std::string& name, const Json& save)
	{
		saves[name] = save.dump(2);
		return true;
	}

	bool MemorySaveStore::Read(const std::string& name, Json& save) const
	{
		const auto found = saves.find(name);
		if (found == saves.end())
		{
			std::cerr << "[Save System] No save named: " << name << "\n";
			return false;
		}

		save = Json::parse(found->second, nullptr, false);
		if (save.is_discarded())
		{
			std::cerr << "[Save System] Save is corrupted: " << name << "\n";
			return false;
		}

		return true;
	}
}
