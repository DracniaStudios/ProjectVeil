#pragma once
#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <raylib.h>
#include <string>
#include <vector>

struct Asset {
	const char* name = "";
	const char* path = "";
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
	
	// UI Decal
	Asset frame;
	
	/// Blocks
	Asset asphalt_01;
	Asset bricks_01;
	Asset bricks_02;
	Asset fabric_01;
	Asset grass_01;
	Asset gravel_01;
	Asset ground_01;
	Asset ice_01;
	Asset marble_01;
	Asset marble_02;
	Asset marble_03;
	Asset metal_01;
	Asset metal_02;
	Asset metal_03;
	Asset metal_04;
	Asset metal_05;
	Asset metal_06;
	Asset metal_07;
	Asset moss_01;
	Asset planks_01;
	Asset plastic_01;
	Asset rock_01;
	Asset tile_01;
	Asset woodFloor_01;
	Asset wood_01;
};

#include <iostream>
inline Asset LoadAsset(const char* name, const char* path)
{
	Asset asset;
	asset.name = name;
	asset.path = path;
	asset.texture = LoadTexture(path);
	std::cout << path << "\n";

	AssetManager::getInstance().assets.push_back(asset);
	return asset;
};

// Pointer is valid until the asset list is modified — only look up after loadAll()
inline Asset* GetAssetPtrByName(const std::string& name)
{
	for (auto& asset : AssetManager::getInstance().assets)
	{
		if (asset.name != nullptr && name == asset.name)
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
