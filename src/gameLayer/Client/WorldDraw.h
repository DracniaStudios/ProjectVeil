#pragma once
#ifndef CLIENT_WORLD_DRAW_H
#define CLIENT_WORLD_DRAW_H

// Drawing the world's objects. Client-only: the simulation's classes hold the
// data these read and nothing here changes what a tick computes.

struct GameObject;
struct Player;
struct Stalker;
namespace SceneManagement { struct Scene; }

namespace Client {

	// A GameObject's collider wires, model and direction markers. Writes
	// model.transform, as GameObject::render3D did. Also what an Entity or an
	// InteractableObject drew.
	void DrawObject3D(GameObject& object);

	// The object, then (when displayRoute is on) its patrol route and belief.
	// The sound events come from `scene`.
	void DrawStalker3D(Stalker& stalker, const SceneManagement::Scene& scene);

	// The carried artifact first, then the player's own object
	void DrawPlayer3D(Player& player);

	// The reticle; the Scene draws it only while gameplay has input
	void DrawReticle(const Player& player);

	// Every object in the map, in ForEachObject's order: GameObjects, then
	// Entities (a Stalker, a Player or any other), then InteractableObjects.
	void DrawWorldObjects3D(SceneManagement::Scene& scene);

}

#endif
