#pragma once
#include <raylib.h>
#include <optional>

struct GameObject;

namespace Client {
	// The game's Sim::Presentation (renderer model and texture handles from the
	// AssetManager), installed for the process. Call before the first
	// object is built.
	void InstallGamePresentation();

	// The local bounds a mesh collider on this object is fitted to: the unit cube for
	// the shared unit cube (usesUnitCube), else the bound model asset's bounds computed at
	// load. Empty when neither exists; the collider is then left as it was.
	std::optional<BoundingBox> LocalBounds(const GameObject& object);
}
