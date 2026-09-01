#include "AssetManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

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

// raylib's tinyobj triangulates each face into a fixed 64-index buffer
// (TINYOBJ_MAX_FACES_PER_F_LINE) and overflows the stack past it — reject
// .obj files with bigger faces instead of crashing (re-export triangulated)
static constexpr int MAX_OBJ_FACE_VERTS = 21; // 3 * 21 stays under the 64-index buffer

static bool objFacesFitLoader(const fs::path& path)
{
	std::ifstream file(path);
	if (!file.is_open()) { return false; }

	std::string line;
	while (std::getline(file, line))
	{
		if (line.size() < 2 || line[0] != 'f' || (line[1] != ' ' && line[1] != '\t')) { continue; }

		std::istringstream tokens(line);
		std::string token;
		int vertexCount = -1; // first token is the "f" keyword
		while (tokens >> token) { vertexCount++; }

		if (vertexCount > MAX_OBJ_FACE_VERTS)
		{
			std::cerr << "Model has a face with " << vertexCount << " vertices (max "
				<< MAX_OBJ_FACE_VERTS << "): " << path.string() << " — re-export triangulated\n";
			return false;
		}
	}
	return true;
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


// Load All Folders in "recursive" mode and repead Load Folders for each direcyort

// Load all supported texture/model files found under RESOURCES_PATH/folder
void AssetManager::loadFolder(const char* folder)
{
	const fs::path root = fs::path(RESOURCES_PATH) / folder;

	std::error_code errorCode;
	if (!fs::is_directory(root, errorCode))
	{
		std::cerr << "Asset folder not found: " << root.string() << "\n";
		return;
	}

	// Check if Folder Exists in the list of folders, if not add it
	if (std::find(folders.begin(), folders.end(), folder) == folders.end())
	{
		folders.push_back(folder);
	}
	
	// Collect first and sort so load order (and asset indices) don't depend on the OS
	std::vector<fs::path> files;
	for (const auto& entry : fs::directory_iterator(root, errorCode))
	{
		// If the entry is a directory, recursively load its contents. The subfolder is
		// built relative to RESOURCES_PATH (not from entry.path(), which is already
		// rooted at RESOURCES_PATH) — recursing with entry.path() double-prepends
		// RESOURCES_PATH on the next call. That only self-corrects when RESOURCES_PATH
		// is absolute (fs::path::operator/ discards the LHS for an absolute RHS), which
		// is true for dev builds but not for PRODUCTION_BUILD's relative "./resources/",
		// where every folder past the first level would silently fail to load.
		if (entry.is_directory())
		{
			const std::string subFolder = (fs::path(folder) / entry.path().filename()).string();
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
			if (lowerExtension(file) == ".obj" && !objFacesFitLoader(file)) { continue; }

			std::cout << "Load Object: " << asset.path << "\n";
			asset.model = LoadModel(asset.path.c_str());

			if (asset.model.meshCount == 0)
			{
				std::cerr << "Failed to load model: " << asset.path << "\n";
				continue;
			}
		}

		if (type == ASSET_TEXTURE)
		{
			std::cout << "Load Texture: " << asset.path << "\n";
			asset.texture = LoadTexture(asset.path.c_str());

			if (asset.texture.id == 0)
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
