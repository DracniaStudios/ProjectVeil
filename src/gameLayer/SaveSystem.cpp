#include <SaveSystem.h>

#include <raylib.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <SceneManager.h>

#include "external/glfw/src/internal.h"

constexpr int VERSION = 1;

namespace SaveSystem
{
	using Json = nlohmann::json;

	bool SaveGame(const char* fileName, void* data, size_t size)
	{
		
		std::ofstream file(fileName, std::ios::binary);

		if (!file.is_open()) { return false; }

		file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));

		file.close();

		return true;
	}

	bool LoadGame(const char* fileName, void* data, size_t size)
	{
		std::ifstream file(fileName, std::ios::binary);

		if (!file.is_open()) { return false; }

		// Get Size Of File
		file.seekg(0, std::ios_base::end);
		size_t readSize = file.tellg();

		// Read Data
		file.seekg(0, std::ios_base::beg);
		file.read(static_cast<char*>(data), std::min(size, readSize));

		auto* data_vec = static_cast<unsigned char*>(data);
		std::vector vec(data_vec, data_vec + size);

		if (!file)
		{
			data = nullptr;
			return false;
		}

		std::cout << data << "\n";

		return true;
	}

	bool SaveWorld(void* data)
	{
		std::error_code errorCode;
		std::filesystem::create_directory(RESOURCES_PATH "../saves", errorCode);

		// Save Block Data To File
		// Files name is based on current level and save name


		// new string
		// add save name to directory
		// add level to file name
		// save in total

		std::filesystem::rename(RESOURCES_PATH "../saves/world.bin.tmp", RESOURCES_PATH "../saves/world.bin.tmp");
		return true;
	}

	bool LoadWorld(void* data)
	{
		GameMap gameMap = {};
		std::unordered_map<int16_t, Entity> entities = {};
		InstanceID entityID = {};
		Player player = {};

		// if original file fails load from back up
		// if back up fails return false and create new save.

		// load files from active save

		// load Game Objects

		// Set Entity IDs and Update Counter
		{
			std::ifstream f(RESOURCES_PATH "../saves/idHolder.txt");

			// Set Entity Ids
			if (!f.is_open()) { return false; }
			f >> entityID.idCounter;
			f.close();
		}

		// Load Player Data
		{
			std::ifstream f(RESOURCES_PATH "../saves/player.txt");
			if (!f.is_open()) { return false; }

			Json j = Json::parse(f, nullptr, /*allow_exceptions=*/false);

			// check if player cal load json else return false
		}

		// Load Entities
		{
			std::ifstream file(RESOURCES_PATH "../saves/entities.txt");
			if (!file.is_open()) { return false; }

			Json j = Json::parse(file, nullptr, /*allow_exceptions*/false);
			for (auto it = j.begin(); it != j.end(); ++it)
			{
				// Check for Entity ID
				// Load Entity By Type
				// Set Instance ID based on saved Entity ID
			}
		}

		return true;
	}

	bool SaveGameObjectsOnly(void* data)
	{
		std::error_code errorCode;
		std::filesystem::create_directory(RESOURCES_PATH "../saves", errorCode);


		// [Optional] Create "Save Only" functions for each type of data that needs to be saved.

		// ID Holder
		{
			std::ofstream file(RESOURCES_PATH "../saves/idHolder.txt.tmp");
			// Get current list of entity id size

			// create new vector 
			// load all entity ids into vector
			// save new vector size

			file.close();
		}

		// Player
		{
			// new Json from player format

			std::ofstream file(RESOURCES_PATH "../saves/player.txt.tmp");
			// dump into file with indent of 2
			file.close();
		}

		//Entities
		{
			Json j;

			/// for each entity in scene
			/// format entitiy into json based on id
			
			std::ofstream file(RESOURCES_PATH "../saves/entities.txt.tmp");
			file << j.dump(2);
			file.close();
			
		}

		// Load All GameObjects and compare their active ID to Player and Entities

		std::filesystem::rename(RESOURCES_PATH "../saves/idHolder.txt.tmp", RESOURCES_PATH "../saves/idHolder.txt");
		std::filesystem::rename(RESOURCES_PATH "../saves/player.txt.tmp", RESOURCES_PATH "../saves/player.txt");
		std::filesystem::rename(RESOURCES_PATH "../saves/entities.txt.tmp", RESOURCES_PATH "../saves/entities.txt");

		return true;
	}
}
