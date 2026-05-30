#pragma once
#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <raylib.h>

struct AssetManager
{
	// Magic Items
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

	// Functions
	void loadAll();
	//void loadTexture(int textureID);
	//void unloadTexture(int textureID);

};

inline AssetManager assets;

inline Texture2D getTextureFromID(int textureID)
{
	switch (textureID)
	{
	case 1: return assets.asphalt_01;
	case 2: return assets.bricks_01;
	case 3: return assets.bricks_02;
	case 4: return assets.fabric_01;
	case 5: return assets.grass_01;
	case 6: return assets.gravel_01;
	case 7: return assets.ground_01;
	case 8: return assets.ice_01;
	case 9: return assets.marble_01;
	case 10: return assets.marble_02;
	case 11: return assets.marble_03;
	case 12: return assets.metal_01;
	case 13: return assets.metal_02;
	case 14: return assets.metal_03;
	case 15: return assets.metal_04;
	case 16: return assets.metal_05;
	case 17: return assets.metal_06;
	case 18: return assets.metal_07;
	case 19: return assets.moss_01;
	case 20: return assets.planks_01;
	case 21: return assets.plastic_01;
	case 22: return assets.rock_01;
	case 23: return assets.tile_01;
	case 24: return assets.woodFloor_01;
	case 25: return assets.wood_01;
	default:return assets.frame;
	}
}

#endif
