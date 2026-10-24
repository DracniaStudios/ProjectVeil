#include "PhysicsWorld.h"

// No engine includes, on purpose: like Collider3D.cpp and RigidBody3D.cpp this
// links against raylib alone, which is what lets tests/PhysicsWorldTests.cpp run
// in the cheap tier. What a contact means for the game is decided by whoever
// reads the events (gameLayer's ContactRules).

namespace Physics3D {

	namespace
	{
		// How far past a body's own box a neighbour still counts toward its touch
		// flags. The cheap box test gates the six raycasts, so distant bodies cost
		// nothing.
		constexpr float kTouchMargin = 0.15f;

		bool IsActive(const RigidBody3D& body)
		{
			return body.isEnabled && body.collider.canCollide;
		}

		// Rebuilds one body's touch flags against every solid body near it. A
		// trigger never sets one, on either side: a trigger that did would be a
		// floor the player could stand and jump on, which is exactly what a damage
		// zone or an objective volume must not be.
		void RebuildTouchFlags(RigidBody3D& body, std::span<const PhysicsBody> bodies)
		{
			body.upTouch = body.downTouch = body.frontTouch = body.backTouch = body.leftTouch = body.rightTouch = false;
			if (!IsActive(body) || body.isStatic || body.collider.isTrigger()) { return; }

			const Vector3 center = body.GetColliderCenter();
			const Vector3 half = body.GetWorldHalfExtents();
			const BoundingBox nearBox = {
				{ center.x - half.x - kTouchMargin, center.y - half.y - kTouchMargin, center.z - half.z - kTouchMargin },
				{ center.x + half.x + kTouchMargin, center.y + half.y + kTouchMargin, center.z + half.z + kTouchMargin }
			};

			for (const PhysicsBody& other : bodies)
			{
				if (other.rigidBody == nullptr || other.rigidBody == &body) { continue; }
				const RigidBody3D& neighbour = *other.rigidBody;
				if (!IsActive(neighbour) || neighbour.collider.isTrigger()) { continue; }

				// Not OverlapsBroadPhase: nearBox is this body's box inflated by the
				// touch margin, not the box itself
				if (!CheckCollisionBoxes(nearBox, neighbour.broadPhaseBox)) { continue; }
				body.checkRayCollision(neighbour);
			}
		}
	}

	PhysicsWorld::PairKey PhysicsWorld::KeyFor(std::uint64_t first, std::uint64_t second)
	{
		return first < second ? PairKey{ first, second } : PairKey{ second, first };
	}

	bool PhysicsWorld::IsTouching(std::uint64_t a, std::uint64_t b) const
	{
		return touching.contains(KeyFor(a, b));
	}

	std::vector<ContactEvent> PhysicsWorld::Solve(std::span<const PhysicsBody> bodies, int iterations)
	{
		iterations = std::clamp(iterations, 1, 8);

		// The narrow-phase result each pair had the last time it was tested this
		// frame. A pair the broad phase skipped keeps the result it had, so one pass
		// where it was not even near can't end a contact on its own.
		std::unordered_map<PairKey, bool, PairKeyHash> overlapping;
		std::unordered_map<PairKey, bool, PairKeyHash> isTriggerPair;

		// Pairs that were not touching after the last Solve and touched this frame,
		// in the order they first touched, measured before anything was pushed
		std::vector<ContactEvent> entered;
		std::unordered_map<PairKey, std::size_t, PairKeyHash> enteredAt;

		for (int iteration = 0; iteration < iterations; ++iteration)
		{
			for (std::size_t i = 0; i < bodies.size(); ++i)
			{
				RigidBody3D* first = bodies[i].rigidBody;
				if (first == nullptr || !IsActive(*first)) { continue; }

				for (std::size_t j = i + 1; j < bodies.size(); ++j)
				{
					RigidBody3D* second = bodies[j].rigidBody;
					if (second == nullptr || second == first || !IsActive(*second)) { continue; }
					if (first->isStatic && second->isStatic) { continue; }
					if (!first->OverlapsBroadPhase(*second)) { continue; }

					// One separating-axis test drives both the report and the push
					const ContactInfo contact = first->getContact(*second);
					const PairKey key = KeyFor(bodies[i].id, bodies[j].id);
					overlapping[key] = contact.hit;
					if (!contact.hit) { continue; }

					const bool isTrigger = first->collider.isTrigger() || second->collider.isTrigger();
					isTriggerPair[key] = isTrigger;

					if (!touching.contains(key) && !enteredAt.contains(key))
					{
						// The event's a is the lower id, so when that is the second body
						// the normal, which points from first toward second, turns round
						const bool firstIsA = bodies[i].id == key.a;
						const RigidBody3D& a = firstIsA ? *first : *second;
						const RigidBody3D& b = firstIsA ? *second : *first;

						ContactEvent event;
						event.a = key.a;
						event.b = key.b;
						event.phase = ContactPhase::Enter;
						event.isTrigger = isTrigger;
						event.normal = firstIsA ? contact.normal : Vector3Negate(contact.normal);
						// Symmetric: swapping a and b negates both the relative velocity
						// and the normal
						event.closingSpeed = Vector3DotProduct(Vector3Subtract(a.GetVelocity(), b.GetVelocity()), event.normal);
						event.faster = Vector3LengthSqr(b.GetVelocity()) > Vector3LengthSqr(a.GetVelocity()) ? key.b : key.a;

						enteredAt[key] = entered.size();
						entered.push_back(event);
					}

					// A trigger has now done everything it does: it reported the overlap.
					// Nothing is pushed, no velocity changes, no friction.
					if (isTrigger) { continue; }

					first->resolveCollision(*second, contact);
					first->SyncBroadPhaseBox();
					second->SyncBroadPhaseBox();
				}
			}
		}

		for (const PhysicsBody& body : bodies)
		{
			if (body.rigidBody != nullptr) { RebuildTouchFlags(*body.rigidBody, bodies); }
		}

		std::unordered_map<PairKey, bool, PairKeyHash> nowTouching;
		for (const auto& [key, hit] : overlapping)
		{
			if (hit) { nowTouching[key] = isTriggerPair[key]; }
		}

		std::vector<ContactEvent> events;

		// What was touching and no longer is, including pairs whose body left the
		// world or stopped taking part
		for (const auto& [key, isTrigger] : touching)
		{
			if (nowTouching.contains(key)) { continue; }

			ContactEvent event;
			event.a = key.a;
			event.b = key.b;
			event.phase = ContactPhase::Exit;
			event.isTrigger = isTrigger;
			events.push_back(event);
		}

		// What started touching. A pair that touched and was already apart again by
		// the end of the frame still touched, so its Enter is followed by its Exit.
		for (const ContactEvent& enter : entered)
		{
			events.push_back(enter);

			const PairKey key{ enter.a, enter.b };
			if (nowTouching.contains(key)) { continue; }

			ContactEvent exit;
			exit.a = enter.a;
			exit.b = enter.b;
			exit.phase = ContactPhase::Exit;
			exit.isTrigger = enter.isTrigger;
			events.push_back(exit);
		}

		touching = std::move(nowTouching);
		return events;
	}
}
