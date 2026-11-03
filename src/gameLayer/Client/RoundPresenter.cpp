#include "RoundPresenter.h"

#include <Scene.h>
#include <Client/LocalPlayer.h>

#include <iostream>

namespace Client {

	void PresentEvent(SceneManagement::Scene& scene, const Sim::GameEvent& event)
	{
		switch (event.type)
		{
		case Sim::GameEventType::PlayerCaught:
			std::cout << "[Client] Slot " << event.slot << (event.slot == LocalSlot() ? " (you)" : "") << " was caught\n";
			break;

		case Sim::GameEventType::RoundOver:
			// A round that ends takes any MiniGame this client had open with it
			scene.QuitMiniGame();
			if (event.outcome == Sim::RoundOutcome::Caught)
			{
				scene.overlays.Close(Overlay::Pause);
				scene.overlays.Open(Overlay::Jumpscare);
				scene.jumpscareSeconds = kJumpscareSeconds;
			}
			else if (event.outcome == Sim::RoundOutcome::Escaped)
			{
				scene.overlays.Close(Overlay::Pause);
				scene.overlays.Open(Overlay::EndCard);
			}
			break;

		case Sim::GameEventType::StationEvent:
			break;
		}
	}

	void UpdatePresentation(SceneManagement::Scene& scene, float frameTime)
	{
		if (scene.jumpscareSeconds <= 0.0f) { return; }
		scene.jumpscareSeconds -= frameTime;
		if (scene.jumpscareSeconds <= 0.0f)
		{
			scene.jumpscareSeconds = 0.0f;
			scene.overlays.Close(Overlay::Jumpscare);
		}
	}
}
