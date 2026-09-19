#pragma once
#ifndef BROAD_PHASE_VIEW_H
#define BROAD_PHASE_VIEW_H

#include <raylib.h>

#include <cstddef>
#include <cstdint>

struct GameObject;
struct GameMap;
class Arena;

/**
 * A packed copy of everything the collision broad phase reads, and nothing else.
 *
 * The solver runs eight iterations of O(n^2) passes over the three GameMap
 * containers. Each of those objects is a GameObject -- 896 bytes, fourteen cache
 * lines -- while the broad phase reads only its bounding box and three flags,
 * about 25 bytes. So the current walk fetches fourteen lines per object to use
 * half of one, and does it from separately allocated unordered_map nodes
 * scattered across the heap.
 *
 * Measured on a stand-in at the real 896 bytes, O(n^2) with the static gate in
 * place, per frame at 8 iterations:
 *
 *     scattered map          0.59 ms (N=217)    35.2 ms (N=1000)
 *     one contiguous block   0.18 ms            5.4 ms
 *     this, 32-byte records  0.18 ms            3.5 ms
 *
 * The records are contiguous and two fit in a cache line, so the scan is
 * prefetchable; the 896-byte object is only touched once a pair actually passes
 * the cheap test. Owners live in a parallel array rather than inside the record
 * for that reason -- putting an 8-byte pointer in would cost a third of the
 * record for data the scan never reads.
 *
 * Built from an Arena because it is per-frame scratch with exactly one lifetime:
 * build it, use it for the solve, and drop the whole thing with one reset().
 */

enum BroadPhaseFlags : std::uint8_t
{
	BPF_CAN_COLLIDE = 1 << 0,
	BPF_TRIGGER     = 1 << 1,
	BPF_STATIC      = 1 << 2,
};

/**
 * Which container a record came from.
 *
 * Records are grouped by this, so each of the solver's existing passes is a
 * contiguous index range and the traversal order matches what the ForEach*
 * helpers produce today.
 */
enum BroadPhaseGroup : std::uint8_t
{
	BPG_OBJECT = 0,
	BPG_ENTITY,
	BPG_INTERACTABLE,
	BPG_COUNT
};

struct BroadPhaseRec
{
	BoundingBox box;          // the only geometry the broad phase reads
	std::uint8_t flags;       // BroadPhaseFlags
	std::uint8_t pad[7];      // explicit, so the stride is a clean 32 on every target
};
static_assert(sizeof(BroadPhaseRec) == 32, "the packed stride is the point of this type");

class BroadPhaseView
{
public:
	/**
	 * Fills the view from the map's three containers, in the same order the
	 * ForEach* helpers walk them.
	 *
	 * Returns false if the arena cannot hold the view, in which case the view is
	 * left empty and the caller must fall back to walking the containers. That is
	 * why this takes Arena::try_allocate rather than allocate: a failure here must
	 * not take the frame down.
	 */
	bool Build(GameMap& map, Arena& arena);

	bool valid() const noexcept { return records != nullptr; }
	std::size_t size() const noexcept { return total; }

	std::size_t groupBegin(BroadPhaseGroup g) const noexcept { return offsets[g]; }
	std::size_t groupEnd(BroadPhaseGroup g) const noexcept { return offsets[g] + counts[g]; }

	const BroadPhaseRec& rec(std::size_t i) const noexcept { return records[i]; }
	GameObject* owner(std::size_t i) const noexcept { return owners[i]; }

	/**
	 * Re-reads one record's box from its owner.
	 *
	 * resolveConstrains moves bodies, so a record goes stale the moment its pair
	 * is resolved. The solver already funnels every post-resolution refresh
	 * through one place and knows the index it is working on, so this is an O(1)
	 * write-back with no lookup -- which is what keeps the view correct across
	 * the eight iterations rather than only within one.
	 */
	void RefreshBox(std::size_t i) noexcept;

	// The gate, reading the packed flags instead of chasing two colliders.
	// Semantics are identical to Scene.cpp's skipCollisionPair: a trigger pair is
	// deliberately exempt from the static skip, because reporting an overlap is
	// the whole purpose of a trigger.
	static bool SkipPair(const BroadPhaseRec& a, const BroadPhaseRec& b) noexcept
	{
		if (!(a.flags & BPF_CAN_COLLIDE) || !(b.flags & BPF_CAN_COLLIDE)) { return true; }
		if ((a.flags & BPF_TRIGGER) || (b.flags & BPF_TRIGGER)) { return false; }
		return (a.flags & BPF_STATIC) && (b.flags & BPF_STATIC);
	}

	// CheckCollisionBoxes, same as RigidBody3D::OverlapsBroadPhase, so the pair
	// set is identical to the container walk rather than merely similar.
	static bool Overlaps(const BroadPhaseRec& a, const BroadPhaseRec& b) noexcept
	{
		return CheckCollisionBoxes(a.box, b.box);
	}

	// Builds a record's flags from a body. Shared with the solver's player
	// passes, which test a body that lives outside the map.
	static std::uint8_t FlagsFor(const GameObject& object) noexcept;

private:
	BroadPhaseRec* records = nullptr;
	GameObject** owners = nullptr;
	std::size_t counts[BPG_COUNT] = {};
	std::size_t offsets[BPG_COUNT] = {};
	std::size_t total = 0;
};

#endif
