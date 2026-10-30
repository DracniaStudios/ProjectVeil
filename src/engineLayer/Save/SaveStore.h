#pragma once
#ifndef SAVE_STORE_H
#define SAVE_STORE_H

#include <filesystem>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

/**
 * Where saves are kept, by name: Game saves, World saves and the player's
 * Settings all go through one. Apart from the SaveSystem's pipeline so a store
 * links with nothing but the JSON library.
 */
namespace SaveSystem
{
	using Json = nlohmann::json;

	class SaveStore
	{
	public:
		virtual ~SaveStore() = default;

		// False when the save could not be kept; whatever was saved under that
		// name before is still there
		virtual bool Write(const std::string& name, const Json& save) = 0;

		// False when there is no save under that name, or it is not JSON
		virtual bool Read(const std::string& name, Json& save) const = 0;
	};

	// Keeps each save in <directory>/<name>.json. A save is written to a temp file
	// first, so a crash mid-save never corrupts the real one, and the previous
	// save is kept beside it as <name>.json.bak.
	class FileSaveStore final : public SaveStore
	{
	public:
		explicit FileSaveStore(std::filesystem::path directory) : directory(std::move(directory)) {}

		bool Write(const std::string& name, const Json& save) override;
		bool Read(const std::string& name, Json& save) const override;

		std::filesystem::path PathOf(const std::string& name) const;

	private:
		std::filesystem::path directory;
	};

	// Keeps each save in memory as the text a file would hold, so a round trip
	// through it loses whatever a round trip through a file would. For tests.
	class MemorySaveStore final : public SaveStore
	{
	public:
		bool Write(const std::string& name, const Json& save) override;
		bool Read(const std::string& name, Json& save) const override;

	private:
		std::unordered_map<std::string, std::string> saves;
	};
}
#endif
