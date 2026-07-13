#include "AssetManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Texture file types picked up when scanning a folder
static bool isTextureFile(const fs::path& path)
{
	std::string ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}

// Material maps (AmbientCG naming) are named after their folder:
//   blocks/Asphalt031/Asphalt031_4K-PNG_Color.png -> "Asphalt031"
// Everything else uses the file stem:
//   icons/frame.png -> "frame"
static std::string assetNameFor(const fs::path& path)
{
	const std::string stem = path.stem().string();

	if (stem.find("_Color") != std::string::npos)
	{
		return path.parent_path().filename().string();
	}
	return stem;
}

void AssetManager::loadFolder(const char* folder, bool recursive)
{
	const fs::path root = fs::path(RESOURCES_PATH) / folder;

	std::error_code errorCode;
	if (!fs::is_directory(root, errorCode))
	{
		std::cerr << "Asset folder not found: " << root.string() << "\n";
		return;
	}

	// Collect first and sort so load order (and asset indices) don't depend on the OS
	std::vector<fs::path> files;
	if (recursive)
	{
		for (const auto& entry : fs::recursive_directory_iterator(root, errorCode))
		{
			if (entry.is_regular_file() && isTextureFile(entry.path())) { files.push_back(entry.path()); }
		}
	}
	else
	{
		for (const auto& entry : fs::directory_iterator(root, errorCode))
		{
			if (entry.is_regular_file() && isTextureFile(entry.path())) { files.push_back(entry.path()); }
		}
	}
	std::sort(files.begin(), files.end());

	for (const auto& file : files)
	{
		Asset asset;
		asset.name = assetNameFor(file);
		asset.path = file.string();

		if (GetAssetPtrByName(asset.name) != nullptr)
		{
			std::cerr << "Duplicate asset name '" << asset.name << "' from " << asset.path << " — skipped\n";
			continue;
		}

		asset.texture = LoadTexture(asset.path.c_str());
		std::cout << asset.path << "\n";

		assets.push_back(asset);
	}
}

void AssetManager::loadAll()
{
	// UI Decals (non-recursive: icons/MagicSphere tilesets stay unloaded)
	loadFolder("icons", false);

	/// Blocks
	loadFolder("blocks", true);
}
