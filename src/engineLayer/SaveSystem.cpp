#include "SaveSystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <LightingSystem.h>
#include <Scene.h>

// 3 added the "Lighting" section. The loader only rejects files NEWER than
// this, so version 2 saves (which have no lights) still load.
constexpr int VERSION = 3;

constexpr const char* GAME_SAVE_PATH = RESOURCES_PATH "../saves/";
constexpr const char* WORLD_SAVE_PATH = RESOURCES_PATH "scenes/";

namespace SaveSystem
{
	namespace fs = std::filesystem;

	static std::string lowerExtension(const fs::path& path)
	{
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return ext;
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

#pragma region Stores

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

#pragma endregion

#pragma region Participants

	SaveParticipant LightingParticipant()
	{
		return SaveParticipant{
			"Lighting",
			[] { return Lighting::LightingSystem::getInstance().formatToJson(); },
			[](const Json* section)
			{
				Lighting::LightingSystem& lighting = Lighting::LightingSystem::getInstance();
				if (section != nullptr) { lighting.loadFromJson(*section); }
				else { lighting.CreateDefaultRig(); }
			},
		};
	}

#pragma endregion

#pragma region Snapshot

	static Json Snapshot(SceneManagement::Scene& scene, SaveScope scope, const std::vector<SaveParticipant>& participants)
	{
		const bool gameScope = scope == SaveScope::Game;
		Json j;

		// Primary Data
		j["Version"] = VERSION;
		if (!gameScope) { j["WorldOnly"] = true; }
		j["IdCounter"] = scene.gameMap.NextId();

		// Scene Flags. Open overlays are not saved: they belong to the session, and
		// a loaded game always starts with none open.
		if (gameScope)
		{
			j["Scene"]["LimitYBounds"] = scene.limitYBounds;
		}

		// Map Data
		j["Map"]["SizeX"] = scene.gameMap.size.x;
		j["Map"]["SizeY"] = scene.gameMap.size.y;
		j["Map"]["SizeZ"] = scene.gameMap.size.z;

		// Sections held outside the Scene, such as Lighting
		for (const SaveParticipant& participant : participants)
		{
			j[participant.key] = participant.snapshot();
		}

		// Game Objects. A projectile lives for a few seconds, so it belongs to the
		// moment a Game save keeps, never to a level.
		Json objects = Json::object();
		scene.gameMap.ForEachGameObject([&](GameObject& obj)
		{
			if (!gameScope && obj.type == OBJECT_PROJECTILE) { return; }
			objects[std::to_string(obj.id)] = obj.formatToJson();
		});
		j["Objects"] = objects;

		// Entities. The player is constructed by Scene_new, not placed in the world,
		// and a Game save writes it in its own section below.
		Json entities = Json::object();
		scene.gameMap.ForEachEntity([&](Entity& entity)
		{
			if (entity.type == OBJECT_PLAYER || entity.kind == ENTITYKIND_PLAYER) { return; }
			entities[std::to_string(entity.id)] = entity.formatToJson();
		});
		j["Entities"] = entities;

		// Interactables
		Json interactables = Json::object();
		scene.gameMap.ForEachInteractable([&](InteractableObject& interactable)
		{
			interactables[std::to_string(interactable.id)] = interactable.formatToJson();
		});
		j["Interactables"] = interactables;

		// Player. No 2D position: the player only has one inside a MiniGame, as that
		// game's cursor, and a running MiniGame is never saved.
		if (gameScope && scene.player != nullptr)
		{
			j["Player"] = scene.player->formatToJson();
		}

		return j;
	}

#pragma endregion

#pragma region Apply

	// Whether a save can be applied at all. Checked before Apply touches the world,
	// so a save turned away here leaves it as it was. Apply reads these fields with
	// value(), which throws on the wrong type, and by then the world is cleared.
	static bool Validate(const Json& save)
	{
		if (!save.is_object())
		{
			std::cerr << "[Save System] Save is not a JSON object.\n";
			return false;
		}

		for (const char* field : { "Version", "IdCounter" })
		{
			if (save.contains(field) && !save[field].is_number())
			{
				std::cerr << "[Save System] Save's " << field << " is not a number.\n";
				return false;
			}
		}

		if (save.value("Version", 0) > VERSION)
		{
			std::cerr << "[Save System] Save version is newer than the game supports.\n";
			return false;
		}

		for (const char* section : { "Scene", "Map", "Objects", "Entities", "Interactables", "Player" })
		{
			if (save.contains(section) && !save[section].is_object())
			{
				std::cerr << "[Save System] Save's " << section << " section is not an object.\n";
				return false;
			}
		}

		for (const char* section : { "Objects", "Entities", "Interactables" })
		{
			if (!save.contains(section)) { continue; }
			for (const auto& [key, object] : save[section].items())
			{
				if (!object.is_object())
				{
					std::cerr << "[Save System] Save's " << section << " entry " << key << " is not an object.\n";
					return false;
				}
			}
		}

		if (save.contains("Map"))
		{
			for (const char* field : { "SizeX", "SizeY", "SizeZ" })
			{
				if (save["Map"].contains(field) && !save["Map"][field].is_number())
				{
					std::cerr << "[Save System] Save's Map " << field << " is not a number.\n";
					return false;
				}
			}
		}

		if (save.contains("Scene") && save["Scene"].contains("LimitYBounds") && !save["Scene"]["LimitYBounds"].is_boolean())
		{
			std::cerr << "[Save System] Save's LimitYBounds is not true or false.\n";
			return false;
		}

		return true;
	}

	static void RestoreGameObjects(Json& section, GameMap& map)
	{
		for (auto& [key, objData] : section.items())
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

			map.RestoreGameObject(std::move(obj));
		}
	}

	static void RestoreEntities(Json& section, GameMap& map)
	{
		for (auto& [key, entData] : section.items())
		{
			// Built through createByKind so a saved Stalker comes back as a Stalker
			// rather than a sliced base Entity — the kind is read before construction
			// because loadFromJson cannot change an object's type. Saves written
			// before "Kind" existed default to ENTITYKIND_NONE, which is what they were.
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

			map.RestoreEntity(std::move(entity));
		}
	}

	static void RestoreInteractables(Json& section, GameMap& map)
	{
		for (auto& [key, intData] : section.items())
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
			map.RestoreInteractable(std::make_unique<InteractableObject>(std::move(interactable)));
		}
	}

	static bool Apply(Json& save, SceneManagement::Scene& scene, SaveScope scope, const std::vector<SaveParticipant>& participants)
	{
		if (!Validate(save)) { return false; }
		const bool gameScope = scope == SaveScope::Game;

		// The player survives the load; everything in the world is replaced. The ids
		// the save handed out stay reserved; each Restore below also keeps the next
		// id above its own.
		scene.ClearWorld();
		scene.gameMap.ReserveIdsBelow(save.value("IdCounter", scene.gameMap.NextId()));

		// Scene Flags
		// Older saves also carry Is2DActive and IsMiniActive; they are ignored, since
		// the overlays those flags stood for are never restored from a save.
		if (gameScope && save.contains("Scene"))
		{
			scene.limitYBounds = save["Scene"].value("LimitYBounds", false);
		}

		// Map Data
		if (save.contains("Map"))
		{
			scene.gameMap.size.x = save["Map"].value("SizeX", scene.gameMap.size.x);
			scene.gameMap.size.y = save["Map"].value("SizeY", scene.gameMap.size.y);
			scene.gameMap.size.z = save["Map"].value("SizeZ", scene.gameMap.size.z);
		}

		// Sections held outside the Scene, such as Lighting
		for (const SaveParticipant& participant : participants)
		{
			participant.apply(save.contains(participant.key) ? &save[participant.key] : nullptr);
		}

		// Interactables name their Target by id, so the order these are restored in
		// doesn't matter
		if (save.contains("Objects")) { RestoreGameObjects(save["Objects"], scene.gameMap); }
		if (save.contains("Entities")) { RestoreEntities(save["Entities"], scene.gameMap); }
		if (save.contains("Interactables")) { RestoreInteractables(save["Interactables"], scene.gameMap); }

		// Player
		if (gameScope && save.contains("Player") && scene.player != nullptr)
		{
			// Older saves also carry Pos2X / Pos2Y, the 2D position the player no longer
			// has; they are ignored.
			if (!scene.player->loadFromJson(save["Player"]))
			{
				std::cerr << "Failed to load Player data.\n";
			}
		}

		return true;
	}

#pragma endregion

	bool Save(SaveStore& store, const std::string& name, SceneManagement::Scene& scene,
		SaveScope scope, const std::vector<SaveParticipant>& participants)
	{
		// Check For Object Limit
		if (scene.gameMap.GameObjectCount() > OBJECT_LIMIT) { return false; }

		return store.Write(name, Snapshot(scene, scope, participants));
	}

	bool Load(const SaveStore& store, const std::string& name, SceneManagement::Scene& scene,
		SaveScope scope, const std::vector<SaveParticipant>& participants)
	{
		Json save;
		if (!store.Read(name, save)) { return false; }

		return Apply(save, scene, scope, participants);
	}

	bool SaveGame(const char* fileName, SceneManagement::Scene* scene)
	{
		if (scene == nullptr) { return false; }

		FileSaveStore store(GAME_SAVE_PATH);
		if (!Save(store, fileName, *scene, SaveScope::Game, { LightingParticipant() })) { return false; }

		const fs::path path = store.PathOf(fileName);
		std::cout << "[Save System] Saved Game: " << path.string() << "\n";
		saveName = path.string();
		return true;
	}

	bool LoadGame(const char* fileName, SceneManagement::Scene& scene)
	{
		const FileSaveStore store(GAME_SAVE_PATH);
		if (!Load(store, fileName, scene, SaveScope::Game, { LightingParticipant() })) { return false; }

		const fs::path path = store.PathOf(fileName);
		std::cout << "[Save System] Loaded Game: " << path.string() << "\n";
		saveName = path.string();
		return true;
	}

	bool SaveWorld(std::string fileName, SceneManagement::Scene* scene)
	{
		if (scene == nullptr) { return false; }

		FileSaveStore store(WORLD_SAVE_PATH);
		if (!Save(store, fileName, *scene, SaveScope::World, { LightingParticipant() })) { return false; }

		std::cout << "[Save System] Saved World: " << store.PathOf(fileName) << "\n";
		return true;
	}

	bool LoadWorld(std::string fileName, SceneManagement::Scene& scene)
	{
		const FileSaveStore store(WORLD_SAVE_PATH);
		std::cout << "[Save System] Attempt Loading World: " << store.PathOf(fileName);
		if (!Load(store, fileName, scene, SaveScope::World, { LightingParticipant() })) { return false; }

		std::cout << "[Save System] Loaded World: " << store.PathOf(fileName)
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
