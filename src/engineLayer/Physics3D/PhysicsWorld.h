#pragma once
#ifndef PHYSICS_WORLD_H
#define PHYSICS_WORLD_H

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

#include <Physics.h>

namespace Physics3D {

	// One body as the world sees it: the id its owner answers to, and its rigid body.
	struct PhysicsBody
	{
		std::uint64_t id = 0;
		RigidBody3D* rigidBody = nullptr;
	};

	enum class ContactPhase { Enter, Exit };

	/**
	 * Two bodies started or stopped touching.
	 *
	 * Reported once per pair, not once per body: `a` and `b` are the pair's two
	 * ids, lower first, and both bodies are equally "in" the contact. A rule that
	 * cares about one kind of body checks both sides.
	 */
	struct ContactEvent
	{
		std::uint64_t a = 0;
		std::uint64_t b = 0;
		ContactPhase phase = ContactPhase::Enter;

		// Either collider is a trigger, so the contact was reported and nothing was
		// pushed.
		bool isTrigger = false;

		// Enter only, measured at the first touch before anything was resolved:
		Vector3 normal = {};        // unit length, from a toward b
		float closingSpeed = 0.0f;  // how fast a and b were approaching along the normal
		std::uint64_t faster = 0;   // whichever of a and b was moving faster (a on a tie)
	};

	/**
	 * The collision half of a physics frame, over every body at once.
	 *
	 * Each body integrates in its owner's own update (RigidBody3D::Update). Solve
	 * then runs after all of them:
	 *
	 *   1. resolves every overlapping pair of active bodies, `iterations` times
	 *   2. rebuilds every body's touch flags against every solid body near it
	 *   3. reports which pairs started or stopped touching since the last Solve
	 *
	 * Every pair follows the same rules, whatever kind of object owns each body.
	 * A body takes part only while it is enabled and can collide; two static
	 * bodies never touch; a pair with a trigger in it is reported and never
	 * pushed, and a trigger never sets a touch flag.
	 *
	 * The world knows bodies only by id, so it needs nothing from the game: what
	 * a contact means (a noise, damage) is decided by whoever reads the events.
	 */
	class PhysicsWorld
	{
	public:
		std::vector<ContactEvent> Solve(std::span<const PhysicsBody> bodies, int iterations = 8);

		// Whether the last Solve left these two bodies touching.
		bool IsTouching(std::uint64_t a, std::uint64_t b) const;

		// Forgets every contact, as for a new world whose ids start over.
		void Reset() { touching.clear(); }

	private:
		// A pair of body ids, lower first
		struct PairKey
		{
			std::uint64_t a = 0;
			std::uint64_t b = 0;
			bool operator==(const PairKey&) const = default;
		};
		struct PairKeyHash
		{
			std::size_t operator()(const PairKey& key) const noexcept
			{
				return std::hash<std::uint64_t>{}(key.a * 0x9E3779B97F4A7C15ull ^ key.b);
			}
		};
		static PairKey KeyFor(std::uint64_t first, std::uint64_t second);

		// The pairs in contact after the last Solve, and whether each one is a
		// trigger pair (an Exit has to say so after one of its bodies is gone)
		std::unordered_map<PairKey, bool, PairKeyHash> touching;
	};
}

#endif
