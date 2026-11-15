#pragma once
#ifndef SIM_WORLDS_H
#define SIM_WORLDS_H
namespace SceneManagement { struct Scene; }
namespace Sim {
	// The Tutorial's world: spawn point (20, 2, 0), then the "chunk_1" World save.
	// Shared by Scene_TutorialConstruct and headless builds, so both compute the same
	// content hash (the spawn point is part of it, WorldKey.cpp:171-173).
	bool LoadTutorialWorld(SceneManagement::Scene& scene);
}
#endif
