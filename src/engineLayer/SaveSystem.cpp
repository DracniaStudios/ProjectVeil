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

	static Json SceneToJson(Scene* scene)
	{
		Json j;

		// Primary Data
		j["Version"] = VERSION;
		j["IdCounter"] = scene->gameMap.instanceHolder.idCounter;

		// Scene Flags
		j["Scene"]["Is2DActive"] = scene->is2DActive;
		j["Scene"]["IsMiniActive"] = scene->isMiniActive;
		j["Scene"]["LimitYBounds"] = scene->limitYBounds;

		// Map Data
		j["Map"]["SizeX"] = scene->gameMap.size.x;
		j["Map"]["SizeY"] = scene->gameMap.size.y;
		j["Map"]["SizeZ"] = scene->gameMap.size.z;

		// Lighting (lights + atmosphere). Lives on the LightingSystem singleton
		// rather than on Scene, the same way the cameras live on SceneManager.
		j["Lighting"] = LightingSystem::getInstance().formatToJson();

		// Game Objects (interactables live in their own section)
		Json objects = Json::object();
		for (auto& obj : scene->gameMap.gameObjects)
		{
			if (scene->gameMap.interactables.contains(obj.id)) { continue; }
			objects[std::to_string(obj.id)] = obj.formatToJson();
		}
		j["Objects"] = objects;

		// Entities (player lives in its own section)
		Json entities = Json::object();
		for (auto& [id, entity] : scene->gameMap.entities)
		{
			if (entity->type == OBJECT_PLAYER) { continue; }
			entities[std::to_string(id)] = entity->formatToJson();
		}
		j["Entities"] = entities;

		// Interactables
		Json interactables = Json::object();
		for (auto& [id, interactable] : scene->gameMap.interactables)
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
			std::cerr << "[Save System] Save version is newer than the game supports.\n";
			return false;
		}

		// Reset Scene Data
		// gameObjects/entities are already empty here — LoadGame() released and
		// cleared them before calling in. interactables is the one container
		// that still holds the previous scene's live objects at this point.
		for (auto& [id, interactable] : scene.gameMap.interactables) { interactable->releaseGeneratedModel(); }
		scene.gameMap.gameObjects.clear();
		scene.gameMap.entities.clear();
		scene.gameMap.interactables.clear();
		// Player::inventory is a list of non-owning pointers into the map just
		// cleared above; the player object itself survives the load, so those
		// pointers would otherwise dangle (inventory contents aren't persisted yet).
		// Player::interactObjectId is the same hazard — ActivateMiniGame() caches
		// an id into this same map for any interactable with the default
		// (0) activator, and nothing else clears it once the map is wiped.
		if (scene.player) {
			scene.player->inventory.clear();
			scene.player->interactObjectId = 0;
		}
		scene.gameMap.instanceHolder.idCounter = j.value("IdCounter", scene.gameMap.instanceHolder.idCounter);

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

		// Lighting — a version 2 save has no "Lighting" section at all, and an
		// unlit world reads as a broken build, so fall back to the default rig
		if (j.contains("Lighting"))
		{
			LightingSystem::getInstance().loadFromJson(j["Lighting"]);
		}
		else
		{
			LightingSystem::getInstance().CreateDefaultRig();
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

				scene.gameMap.gameObjects.push_back(obj);
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

				scene.gameMap.entities[entity->id] = std::move(entity->clone());
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

				// Interactables live only in Scene::interactables, not in gameObjects
				// (see GameMap::saveInteractable/removeInteractable) — pushing the loaded
				// copy into gameObjects here sliced it to a plain GameObject and duplicated
				// the entity under the same ID.
				scene.gameMap.interactables[interactable.id] = std::make_unique<InteractableObject>(interactable);
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

		// Derive the counter floor from the ids the file actually contains.
		// Comparing the counter against gameObjects.size() (as this used to)
		// compares a monotonic id against a container length — they are
		// unrelated, so a save whose objects are numbered above their own count
		// still let the next spawn collide with an id already in use.
		std::uint64_t highestId = 0;
		for (const auto& obj : scene.gameMap.gameObjects) { highestId = std::max(highestId, obj.id); }
		for (const auto& [id, entity] : scene.gameMap.entities) { highestId = std::max(highestId, id); }
		for (const auto& [id, interactable] : scene.gameMap.interactables) { highestId = std::max(highestId, id); }

		scene.gameMap.instanceHolder.idCounter = std::max(scene.gameMap.instanceHolder.idCounter, highestId + 1);

		return true;
	}

	bool SaveGame(const char* fileName, Scene* scene)
	{
		if (scene == nullptr) { return false; }

		// Check For Object Limit
		if (scene->gameMap.gameObjects.size() > OBJECT_LIMIT) { return false; }

		const std::string savePath = RESOURCES_PATH "../saves/";
		const fs::path path = savePath + fileName + ".json";
		if (!WriteJsonAtomic(path, SceneToJson(scene))) { return false; }

		std::cout << "[Save System] Saved Game: " << path.string() << "\n";
		saveName = path.string();
		return true;
	}

	bool LoadGame(const char* fileName, Scene& scene)
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

		// ~GameObject()/~Entity() never free GPU resources (see
		// GameMap::removeObject()); release each object's generated fallback
		// model before these clears erase it, or it leaks.
		for (auto& object : scene.gameMap.gameObjects) { object.releaseGeneratedModel(); }
		for (auto& [id, entity] : scene.gameMap.entities) { entity->releaseGeneratedModel(); }
		scene.gameMap.gameObjects.clear();
		scene.gameMap.entities.clear();
		scene.gameMap.instanceHolder.idCounter = 0;

		if (!ApplyJsonToScene(j, scene)) { return false; }

		std::cout << "[Save System] Loaded Game: " << path.string() << "\n";
		saveName = path.string();
		return true;
	}

	bool SaveWorld(std::string fileName, Scene* scene)
	{
		if (scene == nullptr) { return false; }

		// Check For Object Limit
		if (scene->gameMap.gameObjects.size() > OBJECT_LIMIT) { return false; }

		Json j;
		fs::path path = WORLD_SAVE_PATH + fileName + ".json";

		// Primary Data
		j["Version"] = VERSION;
		j["WorldOnly"] = true;
		j["IdCounter"] = scene->gameMap.instanceHolder.idCounter;

		// Map Data
		j["Map"]["SizeX"] = scene->gameMap.size.x;
		j["Map"]["SizeY"] = scene->gameMap.size.y;
		j["Map"]["SizeZ"] = scene->gameMap.size.z;

		// Spawn point is authored level data. Only written once it has actually
		// been set, so loading an older world stays a no-op rather than
		// teleporting the player to an origin nobody chose.
		if (scene->gameMap.hasSpawnPoint)
		{
			j["Map"]["SpawnX"] = scene->gameMap.spawnPoint.x;
			j["Map"]["SpawnY"] = scene->gameMap.spawnPoint.y;
			j["Map"]["SpawnZ"] = scene->gameMap.spawnPoint.z;
		}

		// Lighting is authored level data, so it belongs in the world file too
		j["Lighting"] = LightingSystem::getInstance().formatToJson();

		// Game Objects (interactables live in their own section, projectiles are transient)
		Json objects = Json::object();
		for (auto& obj : scene->gameMap.gameObjects)
		{
			if (scene->gameMap.interactables.contains(obj.id)) { continue; }
			if (obj.type == OBJECT_PROJECTILE) { continue; }
			objects[std::to_string(obj.id)] = obj.formatToJson();
		}
		j["Objects"] = objects;

		// Interactables
		Json interactables = Json::object();
		for (auto& [id, interactable] : scene->gameMap.interactables)
		{
			interactables[std::to_string(id)] = interactable->formatToJson();
		}
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
		for (auto& [id, entity] : scene->gameMap.entities)
		{
			if (!entity || entity->type == OBJECT_PLAYER) { continue; }
			if (entity->kind == ENTITYKIND_PLAYER) { continue; }
			entities[std::to_string(id)] = entity->formatToJson();
		}
		j["Entities"] = entities;

		if (!WriteJsonAtomic(path, j)) { return false; }

		std::cout << "[Save System] Saved World: " << path << "\n";
		return true;
	}

	bool LoadWorld(std::string fileName, Scene& scene)
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

		// Reset world geometry only — entities and player are untouched
		// (entities live in scene.entities, not gameMap.gameObjects; live projectiles are discarded)
		// ~GameObject()/~Entity()/~InteractableObject() never free GPU resources
		// (see GameMap::removeObject()); release each object's generated
		// fallback model before these clears erase it, or it leaks.
		for (auto& object : scene.gameMap.gameObjects) { object.releaseGeneratedModel(); }
		for (auto& [id, interactable] : scene.gameMap.interactables) { interactable->releaseGeneratedModel(); }
		for (auto& [id, entity] : scene.gameMap.entities) { entity->releaseGeneratedModel(); }
		scene.gameMap.gameObjects.clear();
		scene.gameMap.interactables.clear();
		scene.gameMap.entities.clear();
		// Player::inventory is a list of non-owning pointers into the map just
		// cleared above; the player object itself survives the load, so those
		// pointers would otherwise dangle (inventory contents aren't persisted yet).
		// Player::interactObjectId is the same hazard — ActivateMiniGame() caches
		// an id into this same map for any interactable with the default
		// (0) activator, and nothing else clears it once the map is wiped.
		if (scene.player) {
			scene.player->inventory.clear();
			scene.player->interactObjectId = 0;
		}
		scene.gameMap.instanceHolder.idCounter = 0;

		// Map Data
		if (j.contains("Map"))
		{
			scene.gameMap.size.x = j["Map"].value("SizeX", scene.gameMap.size.x);
			scene.gameMap.size.y = j["Map"].value("SizeY", scene.gameMap.size.y);
			scene.gameMap.size.z = j["Map"].value("SizeZ", scene.gameMap.size.z);

			// Place the player, which nothing else in this function does. Without
			// it the player keeps whatever position it had — on a fresh launch
			// the RigidBody3D default of (0,0,0), which in chunk_1 is inside the
			// artifact display plinth.
			scene.gameMap.hasSpawnPoint = j["Map"].contains("SpawnX");
			if (scene.gameMap.hasSpawnPoint)
			{
				scene.gameMap.spawnPoint = Vector3{
					j["Map"].value("SpawnX", 0.0f),
					j["Map"].value("SpawnY", 0.0f),
					j["Map"].value("SpawnZ", 0.0f)
				};

				if (scene.player)
				{
					scene.player->rigidBody3D.Teleport(scene.gameMap.spawnPoint);
					// Teleporting without clearing velocity carries any fall speed
					// from the previous world into the new one, so the player
					// arrives already accelerating downward.
					scene.player->rigidBody3D.SetVelocity(Vector3Zero());
				}
			}
		}

		// Lighting — pre-lighting world files have no section, so install the
		// default rig rather than leaving the level pitch black
		if (j.contains("Lighting"))
		{
			LightingSystem::getInstance().loadFromJson(j["Lighting"]);
		}
		else
		{
			LightingSystem::getInstance().CreateDefaultRig();
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

				scene.gameMap.gameObjects.push_back(obj);
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

				// Interactables live only in Scene::interactables, not in gameObjects
				// (see GameMap::saveInteractable/removeInteractable) — pushing the loaded
				// copy into gameObjects here sliced it to a plain GameObject and duplicated
				// the entity under the same ID.
				scene.gameMap.interactables[interactable.id] = std::make_unique<InteractableObject>(interactable);
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

				scene.gameMap.entities[entity->id] = std::move(entity);
			}
		}

		// Avoid id collisions with objects created after the load.
		//
		// The stored IdCounter alone is not enough: a world file hand-authored
		// or written by an older build may not carry the key at all, in which
		// case this fell back to the counter zeroed above and the very next
		// object placed in the editor was handed an id another object already
		// answered to. FindGameObjectByID returns the first match, so the
		// duplicate would shadow the original for picking, activator lookup and
		// save/load alike. Deriving the floor from the ids actually present
		// makes the file's own contents authoritative.
		std::uint64_t highestId = 0;
		for (const auto& obj : scene.gameMap.gameObjects) { highestId = std::max(highestId, obj.id); }
		for (const auto& [id, interactable] : scene.gameMap.interactables) { highestId = std::max(highestId, id); }
		// Entities count toward the floor now that a world file carries them;
		// leaving them out would hand a freshly placed object an id a loaded
		// entity already answers to.
		for (const auto& [id, entity] : scene.gameMap.entities) { highestId = std::max(highestId, id); }

		scene.gameMap.instanceHolder.idCounter = std::max({
			scene.gameMap.instanceHolder.idCounter,
			j.value("IdCounter", scene.gameMap.instanceHolder.idCounter),
			highestId + 1
		});

		std::cout << "[Save System] Loaded World: " << path
			<< " (" << scene.gameMap.gameObjects.size() << " objects, "
			<< scene.gameMap.interactables.size() << " interactables, "
			<< scene.gameMap.entities.size() << " entities, next id "
			<< scene.gameMap.instanceHolder.idCounter << ")\n";

		if (scene.gameMap.hasSpawnPoint)
		{
			std::cout << "[Save System]   spawned player at ("
				<< scene.gameMap.spawnPoint.x << ", " << scene.gameMap.spawnPoint.y
				<< ", " << scene.gameMap.spawnPoint.z << ")\n";
		}
		else
		{
			std::cout << "[Save System]   no spawn point in world; player left at its current position\n";
		}
		return true;
	}

	std::vector<std::string> GetSaveFiles() {
		//std::cout << "[Save System] Get Save Files reads All File Names. !!!BUG PRONED CODE!!! \n";
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
