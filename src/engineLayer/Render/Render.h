#pragma once
#ifndef VEIL_RENDER_H
#define VEIL_RENDER_H
#include <raylib.h> // vocabulary types only
#include <cstdint>
#include <optional>
#include <string>

namespace Render {
	bool Init();      // after Platform::Init, before AssetManager::loadAll
	void Shutdown();  // after close_game(), before Platform::Shutdown
	bool IsReady();
	inline constexpr BoundingBox kUnitCubeBounds = { { -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f } };
}
#endif
