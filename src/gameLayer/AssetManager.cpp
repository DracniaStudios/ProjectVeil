#include "AssetManager.h"

void AssetManager::loadAll()
{
	magicSphereSet = LoadTexture(RESOURCES_PATH "icons/MagicSphere/TileSet512x512px.png");
	magicSphere = LoadTexture(RESOURCES_PATH "icons/MagicSphere/512x512px/01.png");
}
