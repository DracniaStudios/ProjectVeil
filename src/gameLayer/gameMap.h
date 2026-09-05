#pragma once
#ifndef GAMEMAP_H
#define GAMEMAP_H

#include <raylib.h>
#include <vector>
#include <type_traits>
#include <Entity.h>

inline constexpr int OBJECT_LIMIT = 10000;

struct InstanceID
{
	std::uint64_t idCounter = 2;
	std::uint64_t getIdAndIncrement();
};

// Calls a visitor with one or more references and honours early exit: a
// visitor returning bool stops the traversal on `false`, a void visitor
// always continues. Lets ForEach*/ForEachObjectPair accept either shape
// without callers having to remember to `return true;` themselves.
namespace GameMapDetail
{
	template <class F, class... Args>
	inline bool InvokeVisitor(F&& fn, Args&... args)
	{
		if constexpr (std::is_invocable_r_v<bool, F, Args&...>)
		{
			return fn(args...);
		}
		else
		{
			fn(args...);
			return true;
		}
	}
}

struct GameMap {

	void create(Vector3 size);

	// Spawn: create and register a new world object into the map's live
	// state (assigns an id, calls onEnable). See CONTEXT.md — distinct from
	// Save, which persists to disk.
	GameObject* SpawnGameObject(GameObject& object);
	Entity* SpawnEntity(Entity& object);
	InteractableObject* SpawnInteractable(InteractableObject& object);

	// Load: insert an already-fully-formed object under the id it already
	// carries, bypassing Spawn*'s id assignment and onEnable() call — for
	// SaveSystem restoring an object exactly as serialized, not creating a
	// new one.
	GameObject* LoadGameObject(GameObject object);
	void LoadEntity(std::unique_ptr<Entity> entity);
	void LoadInteractable(std::unique_ptr<InteractableObject> interactable);

	// Destroy: remove a previously spawned object by id. Keyed by id, not
	// pointer — a pointer into these containers is only guaranteed valid
	// until the next Destroy of that same object.
	void DestroyGameObject(uint64_t id);
	void DestroyEntity(uint64_t id);
	void DestroyInteractable(uint64_t id);

	GameObject* FindGameObject(uint64_t id);
	Entity* FindEntity(uint64_t id);
	InteractableObject* FindInteractable(uint64_t id);

	// Interactables hold pointers to activator objects, making them the
	// first ones required.
	GameObject* FindWorldObject(uint64_t id);

	// The task station whose minigame is currently running, or nullptr for
	// none. Scanned rather than cached: isRunningMiniGame is the flag the
	// Director, the station noise emitter and the editor panel already read,
	// and a second copy of the same fact would be one more thing to keep in
	// step across activate/release/replay.
	InteractableObject* FindRunningStation();

	// Return Data
	Vector3 getMapSize() const { return Vector3(size.x, size.y, size.z); }

	/**
	 * Applies fn to every object in the map, whatever container it lives in.
	 *
	 * The three containers hold three different things — GameObjects by value,
	 * Entities and InteractableObjects by owning pointer — but every caller that
	 * wants to touch "the whole world" wants the GameObject base of each. Doing
	 * that by hand means writing the same three loops at every call site, and the
	 * failure mode is silent: forget the entities loop and the operation just
	 * quietly applies to part of the world. That is exactly how Fit Model To
	 * Collider lost its entities.
	 */
	template <class F>
	void ForEachObject(F&& fn)
	{
		for (auto& [id, object] : gameObjects) { fn(object); }
		for (auto& [id, object] : entities)    { fn(*object); }
		for (auto& [id, object] : interactables) { fn(*object); }
	}

	// Typed traversal over exactly one container. The visitor may return
	// bool (false stops the traversal early) or void (always continues).
	template <class F>
	void ForEachGameObject(F&& fn)
	{
		for (auto& [id, object] : gameObjects)
		{
			if (!GameMapDetail::InvokeVisitor(fn, object)) return;
		}
	}

	// const overload — needed by callers that only hold a `const GameMap*`
	// (e.g. Stalker::DistanceToObstruction, which reads geometry, not state).
	template <class F>
	void ForEachGameObject(F&& fn) const
	{
		for (const auto& [id, object] : gameObjects)
		{
			if (!GameMapDetail::InvokeVisitor(fn, object)) return;
		}
	}

	template <class F>
	void ForEachEntity(F&& fn)
	{
		for (auto& [id, object] : entities)
		{
			if (!GameMapDetail::InvokeVisitor(fn, *object)) return;
		}
	}

	template <class F>
	void ForEachInteractable(F&& fn)
	{
		for (auto& [id, object] : interactables)
		{
			if (!GameMapDetail::InvokeVisitor(fn, *object)) return;
		}
	}

	// Visits every unordered pair of live GameObjects exactly once, with
	// mutable access to both — the shape Scene's O(n^2) collision solver
	// needs. Owned here (rather than reconstructed by nesting
	// ForEachGameObject calls in the caller) so the traversal strategy can
	// change later (e.g. spatial partitioning) without touching Scene.cpp.
	template <class F>
	void ForEachObjectPair(F&& fn)
	{
		for (auto itA = gameObjects.begin(); itA != gameObjects.end(); ++itA)
		{
			auto itB = itA;
			++itB;
			for (; itB != gameObjects.end(); ++itB)
			{
				if (!GameMapDetail::InvokeVisitor(fn, itA->second, itB->second)) return;
			}
		}
	}

	// Same shape as ForEachObjectPair, over entities — Scene's collision
	// solver runs entity-vs-entity as its own pass (distinct rules from
	// gameObject-vs-gameObject: no canCollide/isStatic gate today).
	template <class F>
	void ForEachEntityPair(F&& fn)
	{
		for (auto itA = entities.begin(); itA != entities.end(); ++itA)
		{
			auto itB = itA;
			++itB;
			for (; itB != entities.end(); ++itB)
			{
				if (!GameMapDetail::InvokeVisitor(fn, *itA->second, *itB->second)) return;
			}
		}
	}

	// Same shape again, over interactables.
	template <class F>
	void ForEachInteractablePair(F&& fn)
	{
		for (auto itA = interactables.begin(); itA != interactables.end(); ++itA)
		{
			auto itB = itA;
			++itB;
			for (; itB != interactables.end(); ++itB)
			{
				if (!GameMapDetail::InvokeVisitor(fn, *itA->second, *itB->second)) return;
			}
		}
	}

	// Bulk conditional erase over gameObjects — the shape Scene's end-of-frame
	// pendingDestroy sweep needs (call a side effect, then decide whether to
	// erase), kept distinct from DestroyGameObject: the sweep already calls
	// onDestroy() itself and, matching its existing behavior, does not release
	// a generated model here.
	template <class Pred>
	void EraseGameObjectsIf(Pred&& pred)
	{
		std::erase_if(gameObjects, [&](auto& entry) { return pred(entry.second); });
	}

	// Reassigns every GameObject's id sequentially from a fresh counter and
	// rebuilds this map's keys to match — a plain field mutation would leave
	// the map keyed by stale ids. Entities/interactables are untouched,
	// matching Scene::ResetID's existing scope.
	void ResetGameObjectIds()
	{
		instanceHolder = InstanceID{};
		std::unordered_map<std::uint64_t, GameObject> renumbered;
		renumbered.reserve(gameObjects.size());
		for (auto& [oldId, object] : gameObjects)
		{
			object.id = instanceHolder.getIdAndIncrement();
			renumbered.emplace(object.id, std::move(object));
		}
		gameObjects = std::move(renumbered);
	}

	Vector3 size = {10, 10, 10};

	// PLayer Spawn Point on Load Scene
	Vector3 spawnPoint = {0, 0, 0};
	bool hasSpawnPoint = false;

	// Not part of the container-privacy deepening below: SaveSystem and
	// Scene::ResetID already manipulate the id-floor/reset logic directly,
	// and consolidating that is a separate, later change (SaveSystem owning
	// its own serialize/deserialize invariants) — left public so this pass
	// doesn't preempt it.
	InstanceID instanceHolder = {}; // All IDs Stored

	// Read-only counts for editor/debug display.
	size_t GameObjectCount() const { return gameObjects.size(); }
	size_t EntityCount() const { return entities.size(); }
	size_t InteractableCount() const { return interactables.size(); }

private:
	std::unordered_map<std::uint64_t, GameObject> gameObjects = {};
	std::unordered_map<std::uint64_t, std::unique_ptr<Entity>> entities{}; // Entities
	std::unordered_map<std::uint64_t, std::unique_ptr<InteractableObject>> interactables{}; // Interactables
};

#endif
