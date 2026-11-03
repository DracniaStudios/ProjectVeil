#include "RoundPresenter.h"

#include <Scene.h>
#include <Client/LocalPlayer.h>
#include <Client/StalkerWarnings.h>

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
		{
			if (!scene.stations.IsValidIndex(event.stationIndex)) { break; }
			const Sim::StationEntry& entry = scene.stations.At(event.stationIndex);

			// Everyone hears a station start, wherever it is (G4)
			if (event.stationEvent == Sim::StationEventKind::Granted)
			{
				if (const InteractableObject* station = scene.gameMap.FindInteractable(entry.id)) { PlayStationActivation(station->getPosition()); }
			}

			if (event.slot != LocalSlot()) { break; }

			switch (event.stationEvent)
			{
			case Sim::StationEventKind::Granted:
				// Ours: the MiniGame screen opens. The server times it from now.
				scene.StartMiniGame(entry.game, scene.gameMap.FindInteractable(entry.id), LocalPlayer(scene));
				break;
			case Sim::StationEventKind::Denied:
				std::cout << "[Client] Station " << entry.id << " denied\n";
				break;
			case Sim::StationEventKind::Released:
				// Taken back by the server (a catch, a wipe): the screen goes too
				if (scene.miniGameSession.IsRunning() && scene.miniGameSession.StationId() == entry.id) { scene.QuitMiniGame(); }
				break;
			case Sim::StationEventKind::Completed:
				std::cout << "[Client] Station " << entry.id << " completed\n";
				break;
			default:
				break;
			}
			break;
		}
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
