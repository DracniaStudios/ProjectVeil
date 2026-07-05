#include "SaveSystem.h"

#include <filesystem>
#include <fstream>
#include <SceneManager.h>

constexpr int VERSION = 2;

constexpr const char* WORLD_SAVE_PATH = RESOURCES_PATH "../saves/world.json";
constexpr const char* WORLD_SAVE_BAK = RESOURCES_PATH "../saves/world.json.bak";

namespace SaveSystem
{
	using Json = nlohmann::json;

	// Rebuild renderer data that cannot be serialized (model + texture binding)
	static void RestoreVisuals(GameObject& obj)
	{
		Vector3 scale = obj.rigidBody3D.scale;
		obj.model = LoadModelFromMesh(GenMeshCube(scale.x, scale.y, scale.z));

		if (obj.texture != nullptr)
		{
			obj.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = obj.texture->texture;
		}
	}

	static Json SceneToJson(Scene* scene)
	{
		Json j;

		// Primary Data
		j["Version"] = VERSION;
		j["IdCounter"] = scene->instanceHolder.idCounter;

		// Scene Flags
		j["Scene"]["Is2DActive"] = scene->is2DActive;
		j["Scene"]["IsMiniActive"] = scene->isMiniActive;
		j["Scene"]["LimitYBounds"] = scene->limitYBounds;

		// Map Data
		j["Map"]["SizeX"] = scene->gameMap.size.x;
		j["Map"]["SizeY"] = scene->gameMap.size.y;
		j["Map"]["SizeZ"] = scene->gameMap.size.z;

		// Game Objects (interactables live in their own section)
		Json objects = Json::object();
		for (auto& obj : scene->gameMap.gameObjects)
		{
			if (scene->interactables.contains(obj.id)) { continue; }
			objects[std::to_string(obj.id)] = obj.formatToJson();
		}
		j["Objects"] = objects;

		// Entities (player lives in its own section)
		Json entities = Json::object();
		for (auto& [id, entity] : scene->entities)
		{
			if (entity->type == OBJECT_PLAYER) { continue; }
			entities[std::to_string(id)] = entity->formatToJson();
		}
		j["Entities"] = entities;

		// Interactables
		Json interactables = Json::object();
		for (auto& [id, interactable] : scene->interactables)
		{
			interactables[std::to_string(id)] = interactable->formatToJson();
		}
		j["Interactables"] = interactables;

		// Player
		if (scene->player != nullptr)
		{
			j["Player"] = scene->player->formatToJson();
			j["Player"]["Pos2X"] = scene->player->rigidBody2D.translation.x;
			j["Player"]["Pos2Y"] = scene->player->rigidBody2D.translation.y;
		}

		return j;
	}

	static bool ApplyJsonToScene(Json& j, Scene& scene)
	{
		if (j.value("Version", 0) > VERSION)
		{
			std::cerr << "Save version is newer than the game supports.\n";
			return false;
		}

		// Reset Scene Data
		scene.gameMap.gameObjects.clear();
		scene.entities.clear();
		scene.interactables.clear();
		scene.instanceHolder.idCounter = j.value("IdCounter", scene.instanceHolder.idCounter);

		// Scene Flags
		if (j.contains("Scene"))
		{
			scene.is2DActive = j["Scene"].value("Is2DActive", false);
			scene.isMiniActive = j["Scene"].value("IsMiniActive", false);
			scene.limitYBounds = j["Scene"].value("LimitYBounds", false);
		}

		// Map Data
		if (j.contains("Map"))
		{
			scene.gameMap.size.x = j["Map"].value("SizeX", scene.gameMap.size.x);
			scene.gameMap.size.y = j["Map"].value("SizeY", scene.gameMap.size.y);
			scene.gameMap.size.z = j["Map"].value("SizeZ", scene.gameMap.size.z);
		}

		// Load Game Objects
		if (j.contains("Objects"))
		{
			for (auto& [key, objData] : j["Objects"].items())
			{
				GameObject obj = {};
				obj.id = std::stoull(key);

				if (!obj.loadFromJson(objData))
				{
					std::cerr << "Failed to load GameObject with ID: " << key << "\n";
					continue;
				}

				RestoreVisuals(obj);
				scene.gameMap.gameObjects.push_back(obj);
			}
		}

		// Load Entities
		if (j.contains("Entities"))
		{
			for (auto& [key, entData] : j["Entities"].items())
			{
				auto entity = std::make_unique<Entity>();
				entity->id = std::stoull(key);

				if (!entity->loadFromJson(entData))
				{
					std::cerr << "Failed to load Entity with ID: " << key << "\n";
					continue;
				}

				RestoreVisuals(*entity);
				scene.entities[entity->id] = std::move(entity);
			}
		}

		// Load Interactables
		if (j.contains("Interactables"))
		{
			for (auto& [key, intData] : j["Interactables"].items())
			{
				InteractableObject interactable(INTERACT_MINIGAME);
				interactable.id = std::stoull(key);

				if (!interactable.loadFromJson(intData))
				{
					std::cerr << "Failed to load Interactable with ID: " << key << "\n";
					continue;
				}

				RestoreVisuals(interactable);
				scene.interactables[interactable.id] = std::make_unique<InteractableObject>(interactable);
				scene.gameMap.gameObjects.push_back(interactable);
			}
		}

		// Load Player
		if (j.contains("Player") && scene.player != nullptr)
		{
			if (!scene.player->loadFromJson(j["Player"]))
			{
				std::cerr << "Failed to load Player data.\n";
			}
			else
			{
				scene.player->rigidBody2D.teleport(Vector2{
					j["Player"].value("Pos2X", scene.player->rigidBody2D.translation.x),
					j["Player"].value("Pos2Y", scene.player->rigidBody2D.translation.y)
				});
			}
		}

		return true;
	}

	bool SaveGame(const char* fileName, Scene* scene)
	{
		if (scene == nullptr) { return false; }

		// Check For Object Limit
		if (scene->gameMap.gameObjects.size() > OBJECT_LIMIT) { return false; }

		const std::filesystem::path path = fileName;
		const std::filesystem::path tmpPath = path.string() + ".tmp";
		const std::filesystem::path bakPath = path.string() + ".bak";

		std::error_code errorCode;
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path(), errorCode);
		}

		// Write to a temp file first so a crash mid-save never corrupts the real one
		std::ofstream file(tmpPath, std::ios::binary);
		if (!file.is_open()) { return false; }

		file << SceneToJson(scene).dump(2);
		file.close();

		// Keep the previous save as a backup, then swap the new one in
		std::filesystem::rename(path, bakPath, errorCode);
		std::filesystem::rename(tmpPath, path, errorCode);

		std::cout << "Saved Game: " << path.string() << "\n";
		saveName = path.string();
		return true;
	}

	bool LoadGame(const char* fileName, Scene& scene)
	{
		std::ifstream file(fileName, std::ios::binary);

		if (!file.is_open())
		{
			std::cerr << "Failed to open save file: " << fileName << "\n";
			return false;
		}

		Json j = Json::parse(file, nullptr, false);
		file.close();

		if (j.is_discarded())
		{
			std::cerr << "Save file is corrupted: " << fileName << "\n";
			return false;
		}

		if (!ApplyJsonToScene(j, scene)) { return false; }

		std::cout << "Loaded Game: " << fileName << "\n";
		saveName = fileName;
		return true;
	}
}
