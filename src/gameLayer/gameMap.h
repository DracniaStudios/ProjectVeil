#pragma once
#ifndef GAMEMAP_H
#define GAMEMAP_H

#include <raylib.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <utility>
#include <Entity.h>

inline constexpr int OBJECT_LIMIT = 10000;

// The first id a GameMap hands out: 0 means "no object". 1 was the single
// player's id; players now take ids from Sim::kFirstPlayerId by Slot (sim/Slots.h).
inline constexpr std::uint64_t FIRST_OBJECT_ID = 2;

struct InstanceID
{
	std::uint64_t idCounter = FIRST_OBJECT_ID;
	std::uint64_t getIdAndIncrement();
};

// Calls a visitor with one or more references and honours early exit: a
// visitor returning bool stops the traversal on `false`, a void visitor
// always continues. Lets the ForEach* traversals accept either shape
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

	// A non-owning, non-allocating reference to a callable, so a ForEach can hand
	// its visitor to gameMap.cpp, where the registry lives. Valid only for the
	// duration of the call it is passed to.
	template <class Signature> class FunctionRef;

	template <class R, class... Args>
	class FunctionRef<R(Args...)>
	{
	public:
		template <class F>
			requires (!std::is_same_v<std::remove_cvref_t<F>, FunctionRef> && std::is_invocable_r_v<R, F&, Args...>)
		FunctionRef(F& fn) noexcept
			: object(const_cast<void*>(static_cast<const void*>(std::addressof(fn))))
			, call([](void* target, Args... args) -> R { return (*static_cast<F*>(target))(std::forward<Args>(args)...); })
		{}

		R operator()(Args... args) const { return call(object, std::forward<Args>(args)...); }

	private:
		void* object;
		R (*call)(void*, Args...);
	};
}

/**
 * How the GameMap stores its objects (MultiplayerSplitPlan.md E5; ADR C,
 * docs/review/C-decision.md). Hybrid by design:
 *
 *   On EnTT     every world object is an entity in one entt::registry, kept in
 *               gameMap.cpp; no other file includes EnTT. Its kind is which
 *               storage component it carries: a plain GameObject, an Entity
 *               (player-like or the Stalker) or an InteractableObject. An id
 *               index maps the World save's stable uint64 ids (E-1) to entities.
 *   Not on EnTT the objects' data and behaviour: they are still the GameObject
 *               class tree, with virtual update and render. Players live in the
 *               Scene's Slots.
 *
 * The boundary is this header: Spawn*, Restore*, Destroy, Find* and ForEach*
 * keep their signatures (E-2). Traversal order is part of the contract, pinned
 * by GameMapEcsTests and RenderOffTickTests: within a kind, the newest storage
 * slot first, a freed slot reused last-freed-first; ForEachObject visits
 * GameObjects, then Entities, then Interactables. Hold ids across ticks, never
 * pointers: the sweep after Destroy and every Clear (a load, a Full wipe)
 * invalidate them.
 *
 * Next (ADR C): render runtime state leaves the classes first (C-08, C-09,
 * after the SDL3 swap); transform and collider stay on the classes until Stage 2.
 */

/**
 * The world's objects, and the one lifecycle every kind of object goes
 * through. See CONTEXT.md for Spawn, Restore, Destroy and Clear.
 *
 * GameObjects, Entities and InteractableObjects each live in their own
 * container, but share one id sequence and the same four rules:
 *
 *   Spawn    stores its own copy of a prototype under a fresh id and enables it
 *   Restore  puts a saved object back under its own id, exactly as it was saved
 *   Destroy  marks an object; SweepDestroyed removes it at the end of the frame
 *   Clear    removes everything now and restarts the ids
 *
 * The map owns every model its objects generated, so nothing outside it has to
 * release one before an object goes away.
 */
struct GameMap {

	// Clears the map and lays a floor the size of the world.
	void create(Vector3 size);

	// Spawn: add a new object built from a prototype. The map stores its own copy
	// under a fresh id, gives it a unit scale if it has none, and enables it
	// (onEnable), which binds a model of the copy's own. The prototype is left as
	// it was and keeps whatever model it holds.
	GameObject* SpawnGameObject(const GameObject& prototype);
	Entity* SpawnEntity(const Entity& prototype);
	InteractableObject* SpawnInteractable(const InteractableObject& prototype);

	// Restore: put back an object as it was saved, under the id it carries. It is
	// not enabled, so its saved flags, health and stamina stand: a door saved
	// disabled stays disabled. Its model is bound, its collider follows isEnabled,
	// and the next Spawn gets an id above it. Returns nullptr, and keeps nothing,
	// when another object of any kind already holds that id.
	GameObject* RestoreGameObject(GameObject object);
	Entity* RestoreEntity(std::unique_ptr<Entity> entity);
	InteractableObject* RestoreInteractable(std::unique_ptr<InteractableObject> interactable);

	// Destroy: mark an object of any kind for removal (GameObject::Destroy).
	// Nothing is erased until SweepDestroyed, so this is safe from inside the
	// object's own update or a ForEach. Returns false when no object has that id.
	bool Destroy(std::uint64_t id);

	// Removes every object Destroy marked, of every kind, calling onDestroy on
	// each first. The Scene runs it once a frame, after the updates.
	void SweepDestroyed(SceneManagement::Scene* scene);

	// Clear: release every object's model and remove every object now, then
	// restart the ids at FIRST_OBJECT_ID. Not deferred, so never call it from
	// inside an update or a ForEach. Scene::ClearWorld also empties what the
	// player carries.
	void Clear();

	// The id the next Spawn will get, for saves and the editor.
	std::uint64_t NextId() const { return instanceHolder.idCounter; }

	// Keeps every id below `nextId` from being handed out, the way a save
	// reserves the ids it used. Never lowers the next id.
	void ReserveIdsBelow(std::uint64_t nextId);

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
		// GameObjects, then Entities, then Interactables; the return value is
		// ignored, as it always was
		auto visit = [&fn](GameObject& object) { fn(object); };
		VisitAllObjects(GameMapDetail::FunctionRef<void(GameObject&)>(visit));
	}

	// Typed traversal over exactly one kind. The visitor may return
	// bool (false stops the traversal early) or void (always continues).
	template <class F>
	void ForEachGameObject(F&& fn)
	{
		auto visit = [&fn](GameObject& object) { return GameMapDetail::InvokeVisitor(fn, object); };
		VisitGameObjects(GameMapDetail::FunctionRef<bool(GameObject&)>(visit));
	}

	// const overload — needed by callers that only hold a `const GameMap*`
	// (e.g. Stalker::DistanceToObstruction, which reads geometry, not state).
	template <class F>
	void ForEachGameObject(F&& fn) const
	{
		auto visit = [&fn](const GameObject& object) { return GameMapDetail::InvokeVisitor(fn, object); };
		VisitGameObjectsConst(GameMapDetail::FunctionRef<bool(const GameObject&)>(visit));
	}

	template <class F>
	void ForEachEntity(F&& fn)
	{
		auto visit = [&fn](Entity& entity) { return GameMapDetail::InvokeVisitor(fn, entity); };
		VisitEntities(GameMapDetail::FunctionRef<bool(Entity&)>(visit));
	}

	// const overload — same need as ForEachGameObject's: callers that only
	// hold a `const GameMap*` (Stalker::DistanceToObstruction,
	// SoundField::AudibleLoudnessAt) still need to see entities as
	// obstructions, not just plain GameObjects.
	template <class F>
	void ForEachEntity(F&& fn) const
	{
		auto visit = [&fn](const Entity& entity) { return GameMapDetail::InvokeVisitor(fn, entity); };
		VisitEntitiesConst(GameMapDetail::FunctionRef<bool(const Entity&)>(visit));
	}

	template <class F>
	void ForEachInteractable(F&& fn)
	{
		auto visit = [&fn](InteractableObject& interactable) { return GameMapDetail::InvokeVisitor(fn, interactable); };
		VisitInteractables(GameMapDetail::FunctionRef<bool(InteractableObject&)>(visit));
	}

	// const overload — see ForEachEntity's above.
	template <class F>
	void ForEachInteractable(F&& fn) const
	{
		auto visit = [&fn](const InteractableObject& interactable) { return GameMapDetail::InvokeVisitor(fn, interactable); };
		VisitInteractablesConst(GameMapDetail::FunctionRef<bool(const InteractableObject&)>(visit));
	}

	Vector3 size = {10, 10, 10};

	// PLayer Spawn Point on Load Scene
	//Vector3 spawnPoint = {0, 0, 0};
	//bool hasSpawnPoint = false;

	// Read-only counts for editor/debug display.
	size_t GameObjectCount() const;
	size_t EntityCount() const;
	size_t InteractableCount() const;

	GameMap();
	~GameMap();
	GameMap(const GameMap&) = delete;
	GameMap& operator=(const GameMap&) = delete;
	GameMap(GameMap&&) noexcept;            // a moved-from GameMap may only be destroyed or assigned to
	GameMap& operator=(GameMap&&) noexcept;

private:
	// Whether any object, of any kind, holds this id
	bool IsIdInUse(std::uint64_t id) const;

	// Removes the object stored under an id, whatever its kind
	void Remove(std::uint64_t id);

	InstanceID instanceHolder = {}; // All IDs Stored

	struct Storage; // the entt::registry, the id index and the live count of each kind (gameMap.cpp)
	std::unique_ptr<Storage> storage;

	void VisitAllObjects(GameMapDetail::FunctionRef<void(GameObject&)> visit);
	void VisitGameObjects(GameMapDetail::FunctionRef<bool(GameObject&)> visit);
	void VisitGameObjectsConst(GameMapDetail::FunctionRef<bool(const GameObject&)> visit) const;
	void VisitEntities(GameMapDetail::FunctionRef<bool(Entity&)> visit);
	void VisitEntitiesConst(GameMapDetail::FunctionRef<bool(const Entity&)> visit) const;
	void VisitInteractables(GameMapDetail::FunctionRef<bool(InteractableObject&)> visit);
	void VisitInteractablesConst(GameMapDetail::FunctionRef<bool(const InteractableObject&)> visit) const;
};

#endif
