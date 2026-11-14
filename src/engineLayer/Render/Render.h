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

	// Debug: unlit, batched, inside the world pass
	void DebugLine(Vector3 a, Vector3 b, Color color);
	void DebugSphere(Vector3 center, float radius, Color color);                 // 16 rings, 16 slices
	void DebugSphere(Vector3 center, float radius, int rings, int slices, Color color);
	void DebugSphereWires(Vector3 center, float radius, int rings, int slices, Color color);
	void DebugCube(Vector3 center, Vector3 size, Color color);
	void DebugBoundingBox(BoundingBox box, Color color);
	void DebugCylinder(Vector3 start, Vector3 end, float startRadius, float endRadius, int sides, Color color);
	void DebugCapsule(Vector3 start, Vector3 end, float radius, int slices, int rings, Color color);
	void DebugGrid(int slices, float spacing);

	// 2D: screen coordinates, origin top-left
	void Rect(Rectangle rect, Color color);
	void RectOutline(Rectangle rect, float thickness, Color color);
	void Circle(Vector2 center, float radius, Color color);                       // 36 segments
	void Text(const std::string& text, Vector2 topLeft, int fontSize, Color color); // position truncated to whole pixels
	int  TextWidth(const std::string& text, int fontSize);                       // == raylib MeasureText
}
#endif
