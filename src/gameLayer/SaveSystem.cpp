#include "SaveSystem.h"

#include <filesystem>
#include <fstream>
#include <SceneManager.h>

constexpr int VERSION = 1;

namespace SaveSystem
{
	using Json = nlohmann::json;

	/** Data Parameters **/

	/**  Data Functions **/

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
		//file.read(static_cast<char*>(data), std::min(size, readSize));

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
		auto scene = static_cast<Scene*>(data);
		
		/// Create Directory
		std::error_code errorCode;
		std::filesystem::create_directory(RESOURCES_PATH "../saves", errorCode);

		// Create File
		// Files name is based on current level and save name
		// new string
		// add save name to directory
		// add level to file name
		// save in total
		permaAssertDevelopement(!scene->gameMap.gameObjects.empty());
		std::ofstream file(RESOURCES_PATH "../saves/world.bin.tmp");

		// Check For Object Limit
		if (scene->gameMap.gameObjects.size() > 10000) { return false; }

		// Primary Data
		file.write(std::to_string(VERSION).c_str(), sizeof(VERSION));
		file.write(std::to_string(scene->instanceHolder.idCounter).c_str(), sizeof(scene->instanceHolder.idCounter));
		//file.write(std::to_string(scene->gameMap.gameObjects.size()).c_str(), sizeof(scene->gameMap.gameObjects.size()));
		
		Json j;

		// Save Object By Type
		for (auto& obj : scene->gameMap.gameObjects)
		{
			if (obj.type != OBJECT_GENERIC) continue;
			j[std::to_string(obj.id)] = obj.formatToJson();
			std::cout << "Dump Object: " << obj.id << "\n";

		}

		// Close File
		file << j.dump(2);
		file.close();
		std::filesystem::rename(RESOURCES_PATH "../saves/world.bin.tmp", RESOURCES_PATH "../saves/world.bin", errorCode);
		return true;
	}

	bool LoadWorld(Scene& scene)
	{
		scene.gameMap = {};
		scene.instanceHolder = {};

		
		// if original file fails load from back up
		// if back up fails return false and create new save.
		std::ifstream file(RESOURCES_PATH "../saves/world.bin");
		
		if (!file.is_open())
		{
			std::cerr << "Failed to open world save file. Attempting to load backup...\n";
			file.open(RESOURCES_PATH "../saves/world.bin.bak");
			if (!file.is_open())
			{
				std::cerr << "Failed to open backup world save file. Creating new save...\n";
				return false;
			}
		}
		
		// Load Scene Data
		int readVersion = VERSION;
		file.read(reinterpret_cast<char*>(&readVersion), sizeof(readVersion));
		//file.read((char*)scene.gameMap.gameObjects.size(), sizeof(scene.gameMap.gameObjects));
		//file.read(reinterpret_cast<char*>(&scene.instanceHolder.idCounter), sizeof(scene.instanceHolder.idCounter));
		
		
		// Load Game Objects
		for (auto& obj : scene.gameMap.gameObjects)
		{
			std::uint64_t id = 0;
			file.read(reinterpret_cast<char*>(&id), sizeof(id));
			Json objData;
			file >> objData;
			obj.id = id;
			obj.type = objData.value("type", OBJECT_GENERIC);
			if (!obj.loadFromJson(objData))
			{
				std::cerr << "Failed to load GameObject with ID: " << id << "\n";
				return false;
			}
		}
		
		file.close();
		
		return true;
	}
	/*
	bool LoadGameObject(std::vector<GameObject>& gameObjects, Json& j)
	{
		for (auto it = j.begin(); it != j.end(); ++it)
		{
			std::uint64_t id = std::stoull(it.key());
			Json objData = it.value();
			// Determine Object Type
			int type = objData.value("type", OBJECT_GENERIC);
			GameObject obj;
			obj.id = id;
			obj.type = type;
			if (!obj.loadFromJson(objData))
			{
				std::cerr << "Failed to load GameObject with ID: " << id << "\n";
				return false;
			}
			gameObjects.push_back(obj);
		}
		return true;
	}
	*/
}
