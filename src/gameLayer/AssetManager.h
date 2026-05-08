#pragma once
#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <raylib.h>

struct AssetManager
{

	Texture2D magicSphereSet;
	Texture2D magicSphere;

	// Functions
	void loadAll();

};

#endif
