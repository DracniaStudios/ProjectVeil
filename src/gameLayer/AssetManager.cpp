#include "AssetManager.h"

void AssetManager::loadAll()
{
	frame = LoadTexture(RESOURCES_PATH "icons/frame.png");

	magicSphereSet = LoadTexture(RESOURCES_PATH "icons/MagicSphere/TileSet512x512px.png");
	magicSphere = LoadTexture(RESOURCES_PATH "icons/MagicSphere/512x512px/01.png");
}
