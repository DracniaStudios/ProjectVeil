#include "AssetManager.h"

#include <Render/Render.h>
#include <ResourcePaths.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>


namespace AssetManager {
	namespace fs = std::filesystem;
	
	static std::string lowerExtension(const fs::path& path)
	{
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return ext;
	}

	// Texture file types picked up when scanning a folder
	static bool isTextureFile(const fs::path& path)
	{
		const std::string ext = lowerExtension(path);
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
	}

	// Object/model file types picked up when scanning a folder.
	// Companion files (.mtl materials, textures referenced by them) are loaded
	// by raylib as part of LoadModel and must not become assets of their own.
	static bool isModelFile(const fs::path& path)
	{
		// Objects Don't Animate
		const std::string ext = lowerExtension(path);
		return ext == ".obj" || ext == ".glb" || ext == ".gltf";
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


	// Load All Folders in "recursive" mode and repeat Load Folders for each direction

	// Load all supported texture/model files found under <resources>/folder
	void AssetManager::loadFolder(const char* folder)
	{
		const fs::path& resources = ResourcePaths::Resources();
		const fs::path root = resources / folder;

		std::error_code errorCode;
		if (!fs::is_directory(root, errorCode))
		{
			std::cerr << "Asset folder not found: " << root.string() << "\n";
			return;
		}

		// Collect first and sort so load order (and asset indices) don't depend on the OS
		std::vector<fs::path> files;
		for (const auto& entry : fs::directory_iterator(root, errorCode))
		{
			// If the entry is a directory, recursively load its contents.
			// Recurse with a path relative to the resources folder (not the absolute
			// filesystem path) so `folder`/`asset.folder` stay stable across
			// machines and don't leak the local build path into the editor UI.
			if (entry.is_directory())
			{
				const std::string subFolder = entry.path().lexically_relative(resources).string();
				loadFolder(subFolder.c_str());
			}

			// If the entry is a regular file, add it to the list of files
			if (entry.is_regular_file()) { files.push_back(entry.path()); }
		}
		std::sort(files.begin(), files.end());

		for (const auto& file : files)
		{
			const AssetType type =
				isModelFile(file) ? ASSET_MODEL :
				isTextureFile(file) ? ASSET_TEXTURE :
				ASSET_NONE;

			// Unsupported files (.mtl and friends) are companions, not assets
			if (type == ASSET_NONE) { continue; }

			Asset asset;
			asset.name = assetNameFor(file);
			asset.path = file.string();
			asset.folder = folder;
			asset.type = type;

			if (GetAssetPtrByName(asset.name, asset.type) != nullptr)
			{
				std::cerr << "Duplicate asset name '" << asset.name << "' from " << asset.path << " — skipped\n";
				continue;
			}


			if (type == ASSET_MODEL)
			{
				// The renderer says why a file was refused when it was one it would not load
				const Render::LoadedModel loaded = Render::LoadModelFile(asset.path);
				if (loaded.rejected) { continue; }
				asset.model = loaded.handle;

				if (!asset.model)
				{
					std::cerr << "Failed to load model: " << asset.path << "\n";
					continue;
				}
				asset.localBounds = loaded.localBounds;
			}

			if (type == ASSET_TEXTURE)
			{
				asset.texture = Render::LoadTextureFile(asset.path);

				if (!asset.texture)
				{
					std::cerr << "Failed to load texture: " << asset.path << "\n";
					continue;
				}
			}

			asset.assetID = static_cast<unsigned int>(assets.size());
			assets.push_back(asset);
		}
	}

	void AssetManager::loadAll()
	{
		loadFolder("");
	}
}