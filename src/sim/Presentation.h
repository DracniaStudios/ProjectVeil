#pragma once
#ifndef SIM_PRESENTATION_H
#define SIM_PRESENTATION_H
// Every place the simulation still reaches into presentation (R1 debt), as one
// interface the client installs at start-up. Nothing installed: every call does
// nothing - what a headless build runs with. Main thread only. A new member is a
// reviewed change; this list should only shrink.
struct GameObject;
namespace Sim {
	class Presentation {
	public:
		virtual ~Presentation() = default;
		// Visuals (A-04)
		virtual void BindVisuals(GameObject&) {}         // was GameObject::loadVisuals' body
		virtual void BindTexture(GameObject&) {}         // was GameObject::setTexture after textureName is set
		virtual void FreeGeneratedModel(GameObject&) {}  // releaseGeneratedModel while ownsModel: UnloadModel when meshCount > 0
	};
	// The installed one, or a do-nothing one. Never null.
	Presentation& CurrentPresentation();
	// For the whole process; nullptr goes back to the do-nothing one. Not owned.
	void InstallPresentation(Presentation* presentation);
}
#endif
