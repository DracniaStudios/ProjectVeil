#pragma once
#ifndef RESOURCE_PATHS_H
#define RESOURCE_PATHS_H

#include <filesystem>

// Where the game finds its files at run time. Nothing is baked into the binary at
// compile time, so a build copied to another machine looks beside its own
// executable instead of at the path it was compiled in.
//
//   Resources()  <executable's folder>/resources   scenes, audio, shaders, textures, models
//   Saves()      <executable's folder>/saves       settings, Game saves, the Host's identity and lock
//
// The environment variable VEIL_RESOURCES_DIR names another resources folder, with
// saves/ beside it. Use it to author Worlds straight into the repo's tracked
// resources/scenes, and by the tests, whose executables sit nowhere near a copy.
//
// No raylib: the Settings tests link without it.
namespace ResourcePaths {

	// No trailing separator. Resolved on first use and kept.
	const std::filesystem::path& Resources();
	const std::filesystem::path& Saves();
}

#endif
