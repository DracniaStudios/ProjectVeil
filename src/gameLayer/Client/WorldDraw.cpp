#include "WorldDraw.h"
#include <Platform.h>

#include <Scene.h>
#include <Player.h>
#include <AI/Stalker.h>
#include <InputSystem.h>
#include <Client/DebugDraw.h>
#include <Render/Render.h>

#include <raymath.h>

namespace Client {

	void DrawObject3D(GameObject& object)
	{
		if (!object.isEnabled) { return; }

		if (object.displayCollider)
		{
			// Two volumes, because they are genuinely different things. White is the
			// axis-aligned box the BROAD phase tests; the coloured one is the collider
			// the narrow phase actually tests. They coincide exactly while the object is
			// unrotated and its collider is the default, and every gap between them —
			// from rotation, from a resized collider, from an offset one — is real slack
			// worth being able to see.
			Render::DebugBoundingBox(object.rigidBody3D.broadPhaseBox, WHITE);

			// Colour carries the mode. A trigger and a solid are indistinguishable on
			// screen otherwise and behave completely differently, which is a miserable
			// thing to debug by moving objects around until something stops falling.
			const Collider3D& collider = object.rigidBody3D.collider;
			const Color colliderColor = collider.isTrigger() ? SKYBLUE : GREEN;
			const Vector3 colliderCenter = object.rigidBody3D.GetColliderCenter();

			if (collider.shape == COLLIDER_SPHERE)
			{
				Render::DebugSphereWires(colliderCenter, collider.GetWorldRadius(object.rigidBody3D.scale), 8, 8, colliderColor);
			}
			else
			{
				DrawOrientedBoxWires(colliderCenter,
					Vector3Scale(object.rigidBody3D.GetLocalHalfExtents(), 2.0f),
					object.rigidBody3D.rotation, colliderColor);
			}
		}

		if (object.display3DModel) {
			// Scale + orientation are baked into the transform by the renderer — meshes
			// stay unit-sized, so both the unit cube and .obj models follow rigidBody3D.scale
			const Render::ModelDraw draw = object.MakeModelDraw();
			Render::DrawModel(draw);
			Render::ModelDraw wires = draw;
			wires.tint = BLACK;
			Render::DrawModelWires(wires);
		}

		if (object.displayDirection) {
			/// Show Directions
			Render::DebugSphere(object.rigidBody3D.forward + object.rigidBody3D.translation, 0.1f, RED);
			Render::DebugSphere(object.rigidBody3D.back + object.rigidBody3D.translation, 0.1f, ORANGE);
			Render::DebugSphere(object.rigidBody3D.left + object.rigidBody3D.translation, 0.1f, YELLOW);
			Render::DebugSphere(object.rigidBody3D.right + object.rigidBody3D.translation, 0.1f, GREEN);
			Render::DebugSphere(object.rigidBody3D.up + object.rigidBody3D.translation, 0.1f, BLUE);
			Render::DebugSphere(object.rigidBody3D.down + object.rigidBody3D.translation, 0.1f, PURPLE);
		}
	}

	void DrawStalker3D(Stalker& stalker, const SceneManagement::Scene& scene)
	{
		DrawObject3D(stalker);

		if (!stalker.displayRoute || !stalker.isEnabled) { return; }

		// Patrol route: a node per waypoint and the loop that joins them, drawn
		// closed because PATROL wraps back to index 0 rather than reversing.
		for (std::size_t i = 0; i < stalker.waypoints.size(); ++i)
		{
			const bool isCurrent = (static_cast<int>(i) == stalker.currentWaypoint);
			Render::DebugSphere(stalker.waypoints[i], isCurrent ? 0.45f : 0.3f, isCurrent ? YELLOW : SKYBLUE);
			Render::DebugLine(stalker.waypoints[i], stalker.waypoints[(i + 1) % stalker.waypoints.size()], SKYBLUE);
		}

		// Where the stalker is actually heading right now, which is only the patrol
		// route while it is patrolling — the other three states steer off belief.
		Vector3 target = {};
		bool hasTarget = false;
		switch (stalker.state)
		{
		case STALKER_PATROL:
			if (!stalker.waypoints.empty() && stalker.currentWaypoint < static_cast<int>(stalker.waypoints.size()))
			{
				target = stalker.waypoints[stalker.currentWaypoint];
				hasTarget = true;
			}
			break;
		case STALKER_INVESTIGATE:
		case STALKER_HUNT:
			target = stalker.lastKnownPosition;
			hasTarget = stalker.hasLastKnown;
			break;
		case STALKER_SEARCH_LAST_KNOWN:
			target = stalker.searchArrived ? stalker.searchTarget : stalker.searchAnchor;
			hasTarget = true;
			break;
		default:
			break;
		}

		if (hasTarget)
		{
			// Red for a live pursuit, orange otherwise: the colour says which of the
			// two kinds of target this is without reading the state text.
			const Color colour = (stalker.state == STALKER_HUNT) ? RED : ORANGE;
			Render::DebugLine(stalker.getPosition(), target, colour);
			Render::DebugSphere(target, 0.25f, colour);
		}

		// The belief itself, drawn even when it is not the current target, so a
		// stale last-known position stays visible while the stalker sweeps away
		// from it.
		if (stalker.hasLastKnown) { Render::DebugSphereWires(stalker.lastKnownPosition, 0.6f, 6, 6, MAROON); }

		for (const auto& event : scene.soundField.Events()) {
			Render::DebugSphereWires(event.position, 0.5f, 6, 6, PINK);
		}
	}

	void DrawPlayer3D(Player& player)
	{
		if (!player.isEnabled) { return; }
		if (player.artifact) { DrawObject3D(*player.artifact); }
		DrawObject3D(player);
	}

	void DrawReticle(const Player& player)
	{
		if (!player.isEnabled) return;
		auto inputSystem = &InputSystem::InputSystem::getInstance();

		/// Reticle
		Render::Circle({ static_cast<float>(Platform::ScreenWidth() / 2), static_cast<float>(Platform::ScreenHeight() / 2) }, 10.0f,
			inputSystem->IsActionDown(InputSystem::ACTION_USE_ITEM) ? RED
			: inputSystem->IsActionDown(InputSystem::ACTION_USE_ITEM2) ? RED
			: WHITE);
	}

	void DrawWorldObjects3D(SceneManagement::Scene& scene)
	{
		scene.gameMap.ForEachGameObject([](GameObject& object) { DrawObject3D(object); });

		// The class picks the body, as the virtual call did: a Stalker and a Player
		// draw their own, every other Entity (a plain one, a test's subclass) the
		// GameObject's. dynamic_cast, not Entity::kind: it is the literal equivalent
		// of the virtual call, and the same test Entity::KindMatchesType uses.
		scene.gameMap.ForEachEntity([&scene](Entity& entity) {
			if (auto* stalker = dynamic_cast<Stalker*>(&entity)) { DrawStalker3D(*stalker, scene); }
			else if (auto* player = dynamic_cast<Player*>(&entity)) { DrawPlayer3D(*player); }
			else { DrawObject3D(entity); }
		});

		scene.gameMap.ForEachInteractable([](InteractableObject& object) { DrawObject3D(object); });
	}

}
