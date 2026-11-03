#pragma once
#ifndef CLIENT_LOCAL_PLAYER_H
#define CLIENT_LOCAL_PLAYER_H

// The one place the game asks "which player is mine?" (DedicatedServerPlan.md
// Step 2.9, MultiplayerSplitPlan.md G5). Only client code calls it: the camera,
// Lighting, audio's listener, the UI and the World Editor. The simulation never
// does (rule R2): it knows players only by Slot.

struct Player;
namespace SceneManagement { struct Scene; }

namespace Client {

	// The Slot this game plays as: 0 in single-player and on the Host, the
	// Welcome's slot on a joined client
	int LocalSlot();
	void SetLocalSlot(int slot);

	// This game's own player in `scene`, or nullptr if its Slot is empty
	Player* LocalPlayer(const SceneManagement::Scene& scene);
}

#endif
