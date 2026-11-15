#pragma once
// CPU-side asset decoding for the GL backend (B-19): glTF/GLB models and PNG/BMP images
// into plain structures, with no GL, no SDL and no window. B-20 uploads the result.
//
// The data matches what raylib 6.0's LoadModel and LoadImage produce (the rules follow
// LoadGLTF and LoadImageFromCgltfImage in rmodels.c, LoadImageFromMemory in rtextures.c),
// except where AssetDecode.cpp says otherwise. Backend-private: only the GL backend and
// tests/AssetDecodeTests.cpp include this.
#include <raylib.h> // Color, BoundingBox (types only; raylib is not linked)

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Render::Decode {

struct Image { int width = 0, height = 0, channels = 0; std::vector<std::uint8_t> pixels; }; // 8-bit, top row first
std::optional<Image> LoadImageFile(const std::string& path);                // PNG or BMP, native channels 1-4
std::optional<Image> LoadImageMemory(std::span<const std::uint8_t> bytes);

struct MeshData {
	std::vector<float> positions;          // xyz
	std::vector<float> normals;            // xyz; empty if absent
	std::vector<float> texcoords;          // uv; empty if absent
	std::vector<std::uint8_t> colors;      // rgba8; empty if absent
	std::vector<std::uint32_t> indices;    // empty if not indexed
	int material = 0;                      // into ModelData::materials
};
struct MaterialData { Color diffuseColor = WHITE; std::optional<Image> diffuseImage; };
struct ModelData {
	std::vector<MeshData> meshes;
	std::vector<MaterialData> materials;   // [0] is the default material, as raylib's
	std::optional<BoundingBox> localBounds;
};
std::optional<ModelData> LoadGltf(const std::string& path); // .glb or .gltf

}
