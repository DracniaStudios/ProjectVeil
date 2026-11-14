#pragma once
#ifndef VEIL_RENDER_H
#define VEIL_RENDER_H
#include <Render/RenderTypes.h>
#include <cstdint>
#include <optional>
#include <string>

namespace Render {
	bool Init();      // after Platform::Init, before AssetManager::loadAll
	void Shutdown();  // after close_game(), before Platform::Shutdown: unloads every model and texture it holds
	bool IsReady();

	struct LoadedModel
	{
		ModelHandle handle;                     // none: failed
		std::optional<BoundingBox> localBounds; // union of the meshes' bounds; local units
		bool rejected = false;                  // refused before loading (an .obj face too big); the reason is already on stderr
	};
	LoadedModel   LoadModelFile(const std::string& path);   // .glb .gltf .obj
	TextureHandle LoadTextureFile(const std::string& path); // .png .bmp (.jpg/.tga fail, as in raylib's config)
	ModelHandle   UnitCube();                                // 1x1x1, one material; none before Init
	TextureHandle DiffuseTexture(ModelHandle model, int material = 0); // what that material samples (the default white texture if none of its own)
	std::uintptr_t NativeTextureId(TextureHandle texture);  // GL texture name for ImGui; 0 for none

	void DrawModel(const ModelDraw& draw);
	void DrawModelWires(const ModelDraw& draw);
}
#endif
