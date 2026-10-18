#pragma once
#ifndef OVERLAYS_H
#define OVERLAYS_H

namespace SceneManagement { struct Scene; }

namespace UI {

	/**
	 * Draws what the scene's Overlays call for: the dimming behind them, then the
	 * panel of the one on top. Only the top overlay is drawn, so the Title is
	 * hidden while Settings is open over it.
	 *
	 * Called once a frame from the scene's 2D pass, between the UIEngine's
	 * beginFrame and endFrame. The buttons here open and close overlays on the
	 * scene they are drawn for, never on whichever scene happens to be current.
	 */
	void drawOverlays(SceneManagement::Scene& scene);
}

#endif // OVERLAYS_H
