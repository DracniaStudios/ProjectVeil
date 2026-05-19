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

	// Functions
	void loadAll();

};

#endif
