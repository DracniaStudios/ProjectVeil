#pragma once
#ifndef CONTACT_RULES_H
#define CONTACT_RULES_H

#include <span>

#include <PhysicsWorld.h>

namespace SceneManagement { struct Scene; }

namespace ContactRules
{
	/**
	 * What contacts mean for the game, read from the events PhysicsWorld::Solve
	 * reported this frame.
	 *
	 * Each rule runs once, when a pair starts touching:
	 *
	 *   - a solid impact fast enough to hear is a noise, for the player and for the
	 *     Stalker (a trigger is passed through, not struck, so it makes none)
	 *   - the Stalker touching the player costs the player its baseDamage
	 *   - a projectile hitting the player or an entity damages it, and is spent
	 *
	 * Both sides of every contact are checked, so a rule never depends on which
	 * body the solver happened to visit first.
	 */
	void Apply(SceneManagement::Scene& scene, std::span<const Physics3D::ContactEvent> events);
}

#endif
