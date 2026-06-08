#pragma once
#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <raylib.h>
#include <string>

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

		void loadAll();
		Texture2D magicSphereSet;
		Texture2D magicSphere;

		// UI Decal
		Texture2D frame;

		/// Blocks
		Texture2D asphalt_01;
		Texture2D bricks_01;
		Texture2D bricks_02;
		Texture2D fabric_01;
		Texture2D grass_01;
		Texture2D gravel_01;
		Texture2D ground_01;
		Texture2D ice_01;
		Texture2D marble_01;
		Texture2D marble_02;
		Texture2D marble_03;
		Texture2D metal_01;
		Texture2D metal_02;
		Texture2D metal_03;
		Texture2D metal_04;
		Texture2D metal_05;
		Texture2D metal_06;
		Texture2D metal_07;
		Texture2D moss_01;
		Texture2D planks_01;
		Texture2D plastic_01;
		Texture2D rock_01;
		Texture2D tile_01;
		Texture2D woodFloor_01;
		Texture2D wood_01;

	};

	inline Texture2D getTextureFromID(int textureID)
	{
		switch (textureID)
		{
		case 1: return AssetManager::getInstance().asphalt_01;
		case 2: return AssetManager::getInstance().bricks_01;
		case 3: return AssetManager::getInstance().bricks_02;
		case 4: return AssetManager::getInstance().fabric_01;
		case 5: return AssetManager::getInstance().grass_01;
		case 6: return AssetManager::getInstance().gravel_01;
		case 7: return AssetManager::getInstance().ground_01;
		case 8: return AssetManager::getInstance().ice_01;
		case 9: return AssetManager::getInstance().marble_01;
		case 10: return AssetManager::getInstance().marble_02;
		case 11: return AssetManager::getInstance().marble_03;
		case 12: return AssetManager::getInstance().metal_01;
		case 13: return AssetManager::getInstance().metal_02;
		case 14: return AssetManager::getInstance().metal_03;
		case 15: return AssetManager::getInstance().metal_04;
		case 16: return AssetManager::getInstance().metal_05;
		case 17: return AssetManager::getInstance().metal_06;
		case 18: return AssetManager::getInstance().metal_07;
		case 19: return AssetManager::getInstance().moss_01;
		case 20: return AssetManager::getInstance().planks_01;
		case 21: return AssetManager::getInstance().plastic_01;
		case 22: return AssetManager::getInstance().rock_01;
		case 23: return AssetManager::getInstance().tile_01;
		case 24: return AssetManager::getInstance().woodFloor_01;
		case 25: return AssetManager::getInstance().wood_01;
		default:return AssetManager::getInstance().frame;
		}
	}
#endif
