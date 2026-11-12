#pragma once
namespace Client {
	// The game's Sim::Presentation (raylib models and textures from the AssetManager,
	// the LightingSystem shader), installed for the process. Call before the first
	// object is built.
	void InstallGamePresentation();
}
