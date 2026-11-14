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

	// Frame and passes. The order is BeginFrame, [BeginWorld .. EndWorld], [Begin2D .. End2D], EndFrame
	void BeginFrame(Color clear);              // window framebuffer; colour and depth cleared
	void EndFrame();                           // everything this frame is submitted; ImGui comes after
	void BeginWorld(const Camera3D& camera);   // depth test on; near 0.05, far 4000, aspect of the framebuffer
	void EndWorld();
	void SetDebugDepthTest(bool on);           // flushes batched debug geometry first
	void Begin2D(const Camera2D& camera);
	void End2D();
	void CaptureScreenshot(const std::string& fileName); // PNG of the window as drawn so far, into the working directory

	// Shadow map: a depth-only target the renderer owns. Works whenever a GL context exists, with or without Init
	bool SetShadowMapSize(int size);                    // 0 frees it; false when a map of that size could not be made (none is kept)
	int  ShadowMapSize();                               // the map that exists now, 0 for none
	bool BeginShadowPass(const Camera3D& lightCamera);  // false: no map, nothing to end
	void EndShadowPass();
	Matrix ShadowViewProjection();                      // view * projection of the last shadow pass (raylib row-vector order)

	// Lighting: the renderer owns the lighting shader and every uniform it reads.
	// The frame holds finished numbers; the renderer unpacks and uploads them as given.
	inline constexpr int kMaxLights = 8; // == Lighting::LIGHT_LIMIT == MAX_LIGHTS in lighting.fs

	struct PackedLight {
		int type = 0;            // Lighting::LightType
		Vector3 position = {};
		Vector3 direction = {};  // normalised
		Vector4 color = {};      // rgb 0-1, a = intensity
		float range = 0.0f;
		Vector2 cone = {};       // x = cos(inner), y = cos(outer)
	};

	struct LightingFrame {
		Vector3 viewPosition = {};
		int lightCount = 0;
		PackedLight lights[kMaxLights] = {};
		int shadowLightSlot = -1;   // packed slot owning the shadow map
		Vector4 ambient = {};       // rgb 0-1, a = strength
		Vector4 fogColor = {};      // 0-1
		float fogDensity = 0.0f, exposure = 1.0f, specularStrength = 0.0f, shininess = 1.0f;
		float shadowTexelSize = 1.0f, shadowBias = 0.0f, shadowSoftness = 0.0f;
		bool shadowsEnabled = false; // the shader's gate, already resolved
	};

	void SetLighting(const LightingFrame& frame);
	unsigned int LightingShaderId(); // 0 when the lighting shader failed to load

	// 2D: screen coordinates, origin top-left
	void Rect(Rectangle rect, Color color);
	void RectOutline(Rectangle rect, float thickness, Color color);
	void Circle(Vector2 center, float radius, Color color);                       // 36 segments
	void Text(const std::string& text, Vector2 topLeft, int fontSize, Color color); // position truncated to whole pixels
	int  TextWidth(const std::string& text, int fontSize);                       // == raylib MeasureText
}
#endif
