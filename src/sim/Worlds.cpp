#include "Worlds.h"

#include <Scene.h>
#include <SaveSystem.h>

namespace Sim {
	bool LoadTutorialWorld(SceneManagement::Scene& scene)
	{
		scene.SetSpawnPoint(Vector3(20, 2, 0));
		return SaveSystem::LoadWorld("chunk_1", scene);
	}
}
