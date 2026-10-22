#include "SaveSystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <LightingSystem.h>
#include <SceneManager.h>

// 3 added the "Lighting" section. The loader only rejects files NEWER than
// this, so version 2 saves (which have no lights) still load.
constexpr int VERSION = 3;

constexpr const char* WORLD_SAVE_PATH = RESOURCES_PATH "scenes/";

namespace SaveSystem
{
	using Json = nlohmann::json;
	namespace fs = std::filesystem;

	static std::string lowerExtension(const fs::path& path)
	{
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return ext;
	}

	// Write json to a temp file first so a crash mid-save never corrupts the real one,
	// keeping the previous save as a backup
	static bool WriteJsonAtomic(const fs::path& path, const Json& j)
	{
		const fs::path tmpPath = path.string() + ".tmp";
		const fs::path bakPath = path.string() + ".bak";

		std::error_code errorCode;
		if (path.has_parent_path())
		{
			fs::create_directories(path.parent_path(), errorCode);
		}

		std::ofstream file(tmpPath, std::ios::binary);
		if (!file.is_open()) { return false; }

		file << j.dump(2);
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

	// Save-file object/entity/interactable keys are always written as
	// std::to_string(id) by this file, but a hand-edited or corrupted save can
	// put anything in a JSON key — std::stoull throws on those instead of
	// returning a sentinel, which would otherwise crash a load outright.
	static bool TryParseId(const std::string& key, std::uint64_t& outId)
	{
		try
		{
			size_t consumed = 0;
			outId = std::stoull(key, &consumed);
			return consumed == key.size();
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	static bool ReadJsonFromFile(const char* fileName, Json& out)
	{
		std::ifstream file(fileName, std::ios::binary);

		if (!file.is_open())
		{
			std::cerr << "[Save System] Failed to open save file: " << fileName << "\n";
			return false;
		}

		out = Json::parse(file, nullptr, false);
		file.close();

		if (out.is_discarded())
		{
			std::cerr << "[Save System] Save file is corrupted: " << fileName << "\n";
			return false;
		}

		return true;
	}

	static Json SceneToJson(SceneManagement::Scene* scene)
	{
		Json j;

		// Primary Data
		j["Version"] = VERSION;
		j["IdCounter"] = scene->gameMap.NextId();

		// Scene Flags. Open overlays are not saved: they belong to the session, and
		// a loaded game always starts with none open.
		j["Scene"]["LimitYBounds"] = scene->limitYBounds;

		// Map Data
		j["Map"]["SizeX"] = scene->gameMap.size.x;
		j["Map"]["SizeY"] = scene->gameMap.size.y;
		j["Map"]["SizeZ"] = scene->gameMap.size.z;

		// Lighting (lights + atmosphere). Lives on the LightingSystem singleton
		// rather than on Scene, the same way the cameras live on SceneManager.
		j["Lighting"] = Lighting::LightingSystem::getInstance().formatToJson();

		// Game Objects (interactables live in their own section)
		Json objects = Json::object();
		scene->gameMap.ForEachGameObject([&](GameObject& obj)
		{
			if (scene->gameMap.FindInteractable(obj.id) != nullptr) { return; }
			objects[std::to_string(obj.id)] = obj.formatToJson();
		});
		j["Objects"] = objects;

		// Entities (player lives in its own section)
		Json entities = Json::object();
		scene->gameMap.ForEachEntity([&](Entity& entity)
		{
			if (entity.type == OBJECT_PLAYER) { return; }
			entities[std::to_string(entity.id)] = entity.formatToJson();
		});
		j["Entities"] = entities;

		// Interactables
		Json interactables = Json::object();
		scene->gameMap.ForEachInteractable([&](InteractableObject& interactable)
		{
			interactables[std::to_string(interactable.id)] = interactable.formatToJson();
		});
		j["Interactables"] = interactables;

		// Player
		if (scene->player != nullptr)
		{
			// No 2D position: the player only has one inside a MiniGame, as that
			// game's cursor, and a running MiniGame is never saved
			j["Player"] = scene->player->formatToJson();
		}

		return j;
	}

	static bool ApplyJsonToScene(Json& j, SceneManagement::Scene& scene)
	{
		if (j.value("Version", 0) > VERSION)
		{
			std::cerr << "[Save System] Save version is newer than the game supports.\n";
			return false;
		}

		// The world is already empty here: LoadGame() cleared it before calling in.
		// The ids the save handed out stay reserved; each Restore below also keeps
		// the next id above its own.
		scene.gameMap.ReserveIdsBelow(j.value("IdCounter", scene.gameMap.NextId()));

		// Scene Flags
		// Older saves also carry Is2DActive and IsMiniActive; they are ignored, since
		// the overlays those flags stood for are never restored from a save.
		if (j.contains("Scene"))
		{
			scene.limitYBounds = j["Scene"].value("LimitYBounds", false);
		}

		// Map Data
		if (j.contains("Map"))
		{
			scene.gameMap.size.x = j["Map"].value("SizeX", scene.gameMap.size.x);
			scene.gameMap.size.y = j["Map"].value("SizeY", scene.gameMap.size.y);
			scene.gameMap.size.z = j["Map"].value("SizeZ", scene.gameMap.size.z);
		}

		// Lighting — a version 2 save has no "Lighting" section at all, and an
		// unlit world reads as a broken build, so fall back to the default rig
		if (j.contains("Lighting"))
		{
			Lighting::LightingSystem::getInstance().loadFromJson(j["Lighting"]);
		}
		else
		{
			Lighting::LightingSystem::getInstance().CreateDefaultRig();
		}

		// Load Game Objects
		if (j.contains("Objects"))
		{
			for (auto& [key, objData] : j["Objects"].items())
			{
				GameObject obj = {};
				if (!TryParseId(key, obj.id))
				{
					std::cerr << "[Save System] Skipping GameObject with non-numeric key: " << key << "\n";
					continue;
				}

				if (!obj.loadFromJson(objData))
				{
					std::cerr << "[Save System] Failed to load GameObject with ID: " << key << "\n";
					continue;
				}

				scene.gameMap.RestoreGameObject(std::move(obj));
			}
		}

		// Load Entities
		if (j.contains("Entities"))
		{
			for (auto& [key, entData] : j["Entities"].items())
			{
				// The concrete type has to be chosen before loadFromJson() runs,
				// so read the persisted kind first. Saves written before "Kind"
				// existed default to ENTITYKIND_NONE, which is what they were.
				const auto kind = static_cast<EntityKind>(
					entData.value("Kind", static_cast<int>(ENTITYKIND_NONE)));
				auto entity = Entity::createByKind(kind);
				if (!TryParseId(key, entity->id))
				{
					std::cerr << "[Save System] Skipping Entity with non-numeric key: " << key << "\n";
					continue;
				}

				if (!entity->loadFromJson(entData))
				{
					std::cerr << "[Save System] Failed to load Entity with ID: " << key << "\n";
					continue;
				}

				scene.gameMap.RestoreEntity(std::move(entity));
			}
		}

		// Load Interactables
		if (j.contains("Interactables"))
		{
			for (auto& [key, intData] : j["Interactables"].items())
			{
				std::uint64_t id = 0;
				if (!TryParseId(key, id))
				{
					std::cerr << "[Save System] Skipping Interactable with non-numeric key: " << key << "\n";
					continue;
				}

				InteractableObject interactable(INTERACT_MINIGAME, 0);
				interactable.id = id;

				if (!interactable.loadFromJson(intData))
				{
					std::cerr << "[Save System] Failed to load Interactable with ID: " << key << "\n";
					continue;
				}

				// Interactables live only in GameMap's interactables, not in gameObjects —
				// pushing the loaded copy into gameObjects here sliced it to a plain
				// GameObject and duplicated the entity under the same ID.
				scene.gameMap.RestoreInteractable(std::make_unique<InteractableObject>(std::move(interactable)));
			}
		}

		// Load Player
		if (j.contains("Player") && scene.player != nullptr)
		{
			// Older saves also carry Pos2X / Pos2Y, the 2D position the player no longer
			// has; they are ignored.
			if (!scene.player->loadFromJson(j["Player"]))
			{
				std::cerr << "Failed to load Player data.\n";
			}
		}

		return true;
	}

	bool SaveGame(const char* fileName, SceneManagement::Scene* scene)
	{
		if (scene == nullptr) { return false; }

		// Check For Object Limit
		if (scene->gameMap.GameObjectCount() > OBJECT_LIMIT) { return false; }

		const std::string savePath = RESOURCES_PATH "../saves/";
		const fs::path path = savePath + fileName + ".json";
		if (!WriteJsonAtomic(path, SceneToJson(scene))) { return false; }

		std::cout << "[Save System] Saved Game: " << path.string() << "\n";
		saveName = path.string();
		return true;
	}

	bool LoadGame(const char* fileName, SceneManagement::Scene& scene)
	{
		Json j;

		const std::string savePath = RESOURCES_PATH "../saves/";
		const fs::path path = savePath + fileName + ".json";

		// Check For File
		if (!ReadJsonFromFile(path.string().c_str(), j)) { return false; }

		// Validate before destroying the live scene. ApplyJsonToScene runs this
		// same check, but by then the clears below have already emptied the world
		// — a rejected save left the player staring at nothing. LoadWorld() has
		// always checked in this order.
		if (j.value("Version", 0) > VERSION)
		{
			std::cerr << "[Save System] Save version is newer than the game supports.\n";
			return false;
		}

		// The player survives the load; everything in the world is replaced
		scene.ClearWorld();

		if (!ApplyJsonToScene(j, scene)) { return false; }

		std::cout << "[Save System] Loaded Game: " << path.string() << "\n";
		saveName = path.string();
		return true;
	}

	bool SaveWorld(std::string fileName, SceneManagement::Scene* scene)
	{
		if (scene == nullptr) { return false; }

		// Check For Object Limit
		if (scene->gameMap.GameObjectCount() > OBJECT_LIMIT) { return false; }

		Json j;
		fs::path path = WORLD_SAVE_PATH + fileName + ".json";

		// Primary Data
		j["Version"] = VERSION;
		j["WorldOnly"] = true;
		j["IdCounter"] = scene->gameMap.NextId();

		// Map Data
		j["Map"]["SizeX"] = scene->gameMap.size.x;
		j["Map"]["SizeY"] = scene->gameMap.size.y;
		j["Map"]["SizeZ"] = scene->gameMap.size.z;

		// Lighting is authored level data, so it belongs in the world file too
		j["Lighting"] = Lighting::LightingSystem::getInstance().formatToJson();

		// Game Objects (interactables live in their own section, projectiles are transient)
		Json objects = Json::object();
		scene->gameMap.ForEachGameObject([&](GameObject& obj)
		{
			if (scene->gameMap.FindInteractable(obj.id) != nullptr) { return; }
			if (obj.type == OBJECT_PROJECTILE) { return; }
			objects[std::to_string(obj.id)] = obj.formatToJson();
		});
		j["Objects"] = objects;

		// Interactables
		Json interactables = Json::object();
		scene->gameMap.ForEachInteractable([&](InteractableObject& interactable)
		{
			interactables[std::to_string(interactable.id)] = interactable.formatToJson();
		});
		j["Interactables"] = interactables;

		// Entities. A world file carried none until now, while LoadWorld cleared
		// scene.entities on the way in — so a stalker placed in the editor was
		// silently destroyed by the next world load, taking its authored patrol
		// route with it. Entities placed in a world are level content in exactly
		// the way interactables are, so they are saved the same way.
		//
		// The player is skipped: it is constructed by Scene_new and restored from
		// its own section in game saves, and a world file has no business
		// carrying a second copy of it.
		Json entities = Json::object();
		scene->gameMap.ForEachEntity([&](Entity& entity)
		{
			if (entity.type == OBJECT_PLAYER) { return; }
			if (entity.kind == ENTITYKIND_PLAYER) { return; }
			entities[std::to_string(entity.id)] = entity.formatToJson();
		});
		j["Entities"] = entities;

		if (!WriteJsonAtomic(path, j)) { return false; }

		std::cout << "[Save System] Saved World: " << path << "\n";
		return true;
	}

	bool LoadWorld(std::string fileName, SceneManagement::Scene& scene)
	{

		Json j;
		fs::path path = WORLD_SAVE_PATH + fileName + ".json";
		std::cout << "[Save System] Attempt Loading World: " << path;
		if (!ReadJsonFromFile(path.string().c_str(), j)) { return false; }

		if (j.value("Version", 0) > VERSION)
		{
			std::cerr << "[Save System] World save version is newer than the game supports.\n";
			return false;
		}

		// Reset world geometry only — player is untouched (live projectiles are
		// discarded). The ids the file handed out stay reserved; each Restore below
		// also keeps the next id above its own.
		scene.ClearWorld();
		scene.gameMap.ReserveIdsBelow(j.value("IdCounter", scene.gameMap.NextId()));

		// Map Data
		if (j.contains("Map"))
		{
			scene.gameMap.size.x = j["Map"].value("SizeX", scene.gameMap.size.x);
			scene.gameMap.size.y = j["Map"].value("SizeY", scene.gameMap.size.y);
			scene.gameMap.size.z = j["Map"].value("SizeZ", scene.gameMap.size.z);
		}

		// Lighting — pre-lighting world files have no section, so install the
		// default rig rather than leaving the level pitch black
		if (j.contains("Lighting"))
		{
			Lighting::LightingSystem::getInstance().loadFromJson(j["Lighting"]);
		}
		else
		{
			Lighting::LightingSystem::getInstance().CreateDefaultRig();
		}

		// Load Game Objects
		if (j.contains("Objects"))
		{
			for (auto& [key, objData] : j["Objects"].items())
			{
				GameObject obj = {};
				if (!TryParseId(key, obj.id))
				{
					std::cerr << "[Save System] Skipping GameObject with non-numeric key: " << key << "\n";
					continue;
				}

				if (!obj.loadFromJson(objData))
				{
					std::cerr << "[Save System] Failed to load GameObject with ID: " << key << "\n";
					continue;
				}

				scene.gameMap.RestoreGameObject(std::move(obj));
			}
		}

		// Load Interactables
		if (j.contains("Interactables"))
		{
			for (auto& [key, intData] : j["Interactables"].items())
			{
				std::uint64_t id = 0;
				if (!TryParseId(key, id))
				{
					std::cerr << "[Save System] Skipping Interactable with non-numeric key: " << key << "\n";
					continue;
				}

				InteractableObject interactable(INTERACT_MINIGAME, 0);
				interactable.id = id;

				if (!interactable.loadFromJson(intData))
				{
					std::cerr << "[Save System] Failed to load Interactable with ID: " << key << "\n";
					continue;
				}

				// Interactables live only in GameMap's interactables, not in gameObjects —
				// pushing the loaded copy into gameObjects here sliced it to a plain
				// GameObject and duplicated the entity under the same ID.
				scene.gameMap.RestoreInteractable(std::make_unique<InteractableObject>(std::move(interactable)));
			}
		}

		// Load Entities. Built through createByKind so a saved Stalker comes back
		// as a Stalker rather than a sliced base Entity — the kind is read before
		// construction because loadFromJson cannot change an object's type.
		if (j.contains("Entities"))
		{
			for (auto& [key, entData] : j["Entities"].items())
			{
				const auto kind = static_cast<EntityKind>(
					entData.value("Kind", static_cast<int>(ENTITYKIND_NONE)));
				auto entity = Entity::createByKind(kind);

				if (!TryParseId(key, entity->id))
				{
					std::cerr << "[Save System] Skipping Entity with non-numeric key: " << key << "\n";
					continue;
				}

				if (!entity->loadFromJson(entData))
				{
					std::cerr << "[Save System] Failed to load Entity with ID: " << key << "\n";
					continue;
				}

				scene.gameMap.RestoreEntity(std::move(entity));
			}
		}

		std::cout << "[Save System] Loaded World: " << path
			<< " (" << scene.gameMap.GameObjectCount() << " objects, "
			<< scene.gameMap.InteractableCount() << " interactables, "
			<< scene.gameMap.EntityCount() << " entities, next id "
			<< scene.gameMap.NextId() << ")\n";

		return true;
	}

	std::vector<std::string> GetSaveFiles() {
		std::error_code errorCode;
		std::vector<std::string> fileNames;
		
		// Get All Files in Worlds Folder
		for (const auto& entry : fs::recursive_directory_iterator(WORLD_SAVE_PATH, errorCode)) {
			// Check Only For Json Files
			const std::string ext = lowerExtension(entry.path());
			if (ext != ".json") { continue; }

			// What Defines a World File?

			if (entry.is_regular_file()) { fileNames.push_back(entry.path().stem().string()); }
		}

		return fileNames;
	}

}
