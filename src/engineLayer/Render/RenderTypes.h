#pragma once
#ifndef VEIL_RENDER_TYPES_H
#define VEIL_RENDER_TYPES_H
// The plain types the simulation holds and passes to the renderer. No functions:
// the simulation library (veil_sim) includes this and never Render.h, so it links
// no renderer.
#include <raylib.h> // vocabulary types only
#include <cstdint>

namespace Render {
	struct ModelHandle
	{
		std::uint32_t id = 0;
		explicit operator bool() const { return id != 0; }
		friend bool operator==(ModelHandle, ModelHandle) = default;
	};

	struct TextureHandle
	{
		std::uint32_t id = 0;
		explicit operator bool() const { return id != 0; }
		friend bool operator==(TextureHandle, TextureHandle) = default;
	};

	// One model draw: where, how big, which way round, and in what colour
	struct ModelDraw
	{
		ModelHandle model;
		TextureHandle diffuseOverride;          // replaces material 0's diffuse texture for this draw
		Vector3 translation = { 0, 0, 0 };
		Quaternion rotation = { 0, 0, 0, 1 };
		Vector3 scale = { 1, 1, 1 };
		Color tint = WHITE;                     // per channel: material colour * tint / 255 (integer)
	};

	inline constexpr BoundingBox kUnitCubeBounds = { { -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f } };
}
#endif
