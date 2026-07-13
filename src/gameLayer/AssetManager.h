#pragma once
#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <raylib.h>
#include <string>
#include <vector>

struct Asset {
	std::string name = "";
	std::string path = "";
	Texture2D texture = {};
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
	void loadAll();

	// Loads every supported texture file found under RESOURCES_PATH/folder
	void loadFolder(const char* folder, bool recursive);
};

#include <iostream>

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

inline Asset GetAssetByName(const std::string& name)
{
	for (const auto& asset : AssetManager::getInstance().assets)
	{
		if (asset.name == name)
		{
			std::cout << asset.path << "\n";
			return asset;
		}
	}
	return Asset();
};

#endif
