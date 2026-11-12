#pragma once
#ifndef CLIENT_ROUND_PRESENTER_H
#define CLIENT_ROUND_PRESENTER_H

// How this client shows what the server reports about the round: the
// Jumpscare after a catch, the end card after an escape. Client code only; the
// same calls serve single-player (events through Loopback) and a joined client.

#include <sim/GameEvents.h>

namespace SceneManagement { struct Scene; }

namespace Client {

	// How long the Jumpscare stays up after a catch, in seconds
	inline constexpr float kJumpscareSeconds = 2.0f;

	// Shows one event from the server on this client's screen
	void PresentEvent(SceneManagement::Scene& scene, const Sim::GameEvent& event);

	// Per frame: takes the Jumpscare down once its time is up
	void UpdatePresentation(SceneManagement::Scene& scene, float frameTime);
}

#endif
