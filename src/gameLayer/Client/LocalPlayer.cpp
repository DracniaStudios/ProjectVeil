#include "LocalPlayer.h"

#include <Scene.h>

namespace Client {

	namespace {
		int localSlot = 0;
	}

	int LocalSlot() { return localSlot; }

	void SetLocalSlot(int slot)
	{
		localSlot = Sim::IsValidSlot(slot) ? slot : 0;
	}

	Player* LocalPlayer(const SceneManagement::Scene& scene)
	{
		return scene.PlayerInSlot(localSlot);
	}
}
