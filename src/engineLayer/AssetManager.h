#pragma once
#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <raylib.h>
#include <string>
#include <vector>

enum AssetType {
	ASSET_NONE,
	ASSET_TEXTURE,
	ASSET_MODEL,
};

struct Asset {
	unsigned int assetID = 0;
	std::string name = "";
	std::string path = "";
	std::string folder = "";
	AssetType type = ASSET_NONE;

	Texture2D texture = {};
	Model model = {};
};

class AssetManager
{
	AssetManager() = default;
	~AssetManager() = default;
public:

	// Delete, copy, and move functions to prevent duplication
	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;
	AssetManager(AssetManager&&) = delete;
	AssetManager& operator=(AssetManager&&) = delete;
	static AssetManager& getInstance()
	{
		static AssetManager instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	// Possible Bug Pronned
	std::vector<Asset> assets;
	std::vector<std::string> folders;
	void loadAll();

	// Loads every supported texture/model file found under RESOURCES_PATH/folder
	void loadFolder(const char* folder);
};

// Pointer is valid until the asset list is modified — only look up after loadAll()
inline Asset* GetAssetPtrByName(const std::string& name)
{
	for (auto& asset : AssetManager::getInstance().assets)
	{
		if (!asset.name.empty() && name == asset.name)
		{
			return &asset;
		}
	}
	return nullptr;
};

// Type-aware lookup — a model and a texture may legally share a name
// (e.g. "Crate" the .obj and "Crate" the .png)
inline Asset* GetAssetPtrByName(const std::string& name, AssetType type)
{
	for (auto& asset : AssetManager::getInstance().assets)
	{
		if (asset.type == type && !asset.name.empty() && name == asset.name)
		{
			return &asset;
		}
	}
	return nullptr;
};

#endif
