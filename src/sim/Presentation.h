#pragma once
#ifndef SIM_PRESENTATION_H
#define SIM_PRESENTATION_H
// Every place the simulation still reaches into presentation (R1 debt), as one
// interface the client installs at start-up. Nothing installed: every call does
// nothing - what a headless build runs with. Main thread only. A new member is a
// reviewed change; this list should only shrink.
#include <raylib.h>      // Vector3 only: nothing here calls raylib
#include <string_view>
struct GameObject;
namespace SceneManagement { struct Scene; }
namespace Sim {
	class Presentation {
	public:
		virtual ~Presentation() = default;
		// Visuals (A-04)
		// The one member with an effect the simulation reads: the client's version fits a
		// COLLIDER_MESH collider to the bound model. With nothing installed a mesh collider
		// keeps the size and offset its World save recorded (decision D7); do not "fix" the
		// headless digest by fitting to a cube.
		virtual void BindVisuals(GameObject&) {}         // was GameObject::loadVisuals' body
		virtual void BindTexture(GameObject&) {}         // was GameObject::setTexture after textureName is set
		virtual void FreeGeneratedModel(GameObject&) {}  // releaseGeneratedModel while ownsModel: UnloadModel when meshCount > 0
		// Sound (A-05)
		virtual void PlayGameplaySoundAt(std::string_view, Vector3, float) {} // ContactRules.cpp:44
		virtual void PlayObjectSound(GameObject&) {}     // InteractableObject.cpp:103-110
		virtual void FollowObjectSound(GameObject&) {}   // GameObject.cpp:230-236
		// Scene lifecycle (A-08)
		virtual void OnFullWipe(SceneManagement::Scene&) {}       // Scene.cpp:275-277
		virtual void OnSceneDeleting(SceneManagement::Scene&) {}  // Scene.cpp:42,47
	};
	// The installed one, or a do-nothing one. Never null.
	Presentation& CurrentPresentation();
	// For the whole process; nullptr goes back to the do-nothing one. Not owned.
	void InstallPresentation(Presentation* presentation);
}
#endif
