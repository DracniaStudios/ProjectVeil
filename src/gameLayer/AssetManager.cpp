#include "AssetManager.h"

void AssetManager::loadAll()
{
	frame = LoadTexture(RESOURCES_PATH "icons/frame.png");

	magicSphereSet = LoadTexture(RESOURCES_PATH "icons/MagicSphere/TileSet512x512px.png");
	magicSphere = LoadTexture(RESOURCES_PATH "icons/MagicSphere/512x512px/01.png");

	/// Blocks
	asphalt_01 = LoadTexture(RESOURCES_PATH "blocks/Asphalt031/Asphalt031_4K-PNG_Color.png");
	bricks_01 = LoadTexture(RESOURCES_PATH "blocks/Bricks058/Bricks058_4K-PNG_Color.png");
	bricks_02 = LoadTexture(RESOURCES_PATH "blocks/Bricks097/Bricks097_4K-PNG_Color.png");
	fabric_01 = LoadTexture(RESOURCES_PATH "blocks/Fabric082/Fabric082_4K-PNG_Color.png");
	grass_01 = LoadTexture(RESOURCES_PATH "blocks/Grass001/Grass001_4K-PNG_Color.png");
	gravel_01 = LoadTexture(RESOURCES_PATH "blocks/Gravel041/Gravel041_4K-PNG_Color.png");
	ground_01 = LoadTexture(RESOURCES_PATH "blocks/Ground088/Ground088_4K-PNG_Color.png");
	ice_01 = LoadTexture(RESOURCES_PATH "blocks/Ice002/Ice002_4K-PNG_Color.png");
	marble_01 = LoadTexture(RESOURCES_PATH "blocks/Marble016/Marble016_4K-PNG_Color.png");
	marble_02 = LoadTexture(RESOURCES_PATH "blocks/Marble021/Marble021_4K-PNG_Color.png");
	marble_03 = LoadTexture(RESOURCES_PATH "blocks/Marble023/Marble023_4K-PNG_Color.png");
	metal_01 = LoadTexture(RESOURCES_PATH "blocks/Metal030/Metal030_4K-PNG_Color.png");
	metal_02 = LoadTexture(RESOURCES_PATH "blocks/Metal046B/Metal046B_4K-PNG_Color.png");
	metal_03 = LoadTexture(RESOURCES_PATH "blocks/Metal049A/Metal049A_4K-PNG_Color.png");
	metal_04 = LoadTexture(RESOURCES_PATH "blocks/Metal052B/Metal052B_4K-PNG_Color.png");
	metal_05 = LoadTexture(RESOURCES_PATH "blocks/Metal053A/Metal053A_4K-PNG_Color.png");
	metal_06 = LoadTexture(RESOURCES_PATH "blocks/Metal054A/Metal054A_4K-PNG_Color.png");
	metal_07 = LoadTexture(RESOURCES_PATH "blocks/Metal056A/Metal056A_4K-PNG_Color.png");
	moss_01 = LoadTexture(RESOURCES_PATH "blocks/Moss002/Moss002_4K-PNG_Color.png");
	planks_01 = LoadTexture(RESOURCES_PATH "blocks/Planks020/Planks020_4K-PNG_Color.png");
	plastic_01 = LoadTexture(RESOURCES_PATH "blocks/Plastic013A/Plastic013A_4K-PNG_Color.png");
	rock_01 = LoadTexture(RESOURCES_PATH "blocks/Rock058/Rock058_4K-PNG_Color.png");
	tile_01 = LoadTexture(RESOURCES_PATH "blocks/Tiles133A/Tiles133A_4K-PNG_Color.png");
	woodFloor_01 = LoadTexture(RESOURCES_PATH "blocks/WoodFloor009/WoodFloor009_4K-PNG_Color.png");
	wood_01 = LoadTexture(RESOURCES_PATH "blocks/Wood067/Wood067_4K-PNG_Color.png");
}