# Stalker Entity — Implementation Plan

## Context

Project Veil's current milestone is the **Vertical slice** (Notion, target 1 Oct 2026): one room, one task station, sound-only AI perception, one stalker enemy. Its Definition of done requires the player to be *hunted by a single stalker enemy driven by a 4-state FSM on sound-only perception, with a Director hinting the AI without omniscience*.

The Stalker is the core of that slice — Notion's task row says `Displaces: "Nothing — core of the slice."` `Changelog.txt` agrees: *"Create The AI for the Entity, this will create the challenge for the character and begin the vertical slice."*

Four design decisions are **locked** (changing any escalates to Kuma before work starts):
1. 4-state FSM: `patrol → investigate → hunt → search-last-known-position`
2. Perception is **sound only** — no vision cone, no omniscient position feed
3. A **Director** hints the AI without giving it ground truth
4. Task-tampering forces re-traversal

### What actually exists vs. what Notion assumes

Notion budgets 28h for the Stalker and 16h for perception, and lists expected paths `src/ai/`, `src/director/`, `src/perception/`. **None of those directories exist.** The repo is C++23/raylib with everything under `src/gameLayer/`. There is no perception layer, no pathfinding, no navmesh, no waypoints, and no AI of any kind — `rg` for `navmesh|pathfind|patrol|waypoint` returns only two unrelated comments in `RigidBody3D.cpp`.

What does exist and should be reused:
- `src/gameLayer/Entity.h` — `Entity : GameObject` with health/stamina/speed, `isCrouching`/`isSprinting`, `Attack()`, JSON hooks, and a `CooldownTimer` buff system that **already declares `BUFF_HEARING` and `BUFF_SEARCH`** — clearly reserved for this work.
- `src/gameLayer/Scene.h:75` — `std::unordered_map<uint64_t, std::unique_ptr<Entity>> entities`
- `src/gameLayer/Scene.cpp:309` — `entity->second->update(scene, delta)` is **already a virtual call through `unique_ptr<Entity>`**. Once construction stops slicing, a `Stalker` subclass ticks with no change to the scene loop.
- `src/gameLayer/AudioManager.h` — FMOD singleton with `Play3D` / `PlayEvent3D` taking a `GameObject&`
- `src/gameLayer/Components/RigidBody3D.cpp:117,430` — working raycasts, already used for collision; reuse for sound occlusion and obstacle avoidance
- `src/gameLayer/WorldEditorTools/` — ImGui inspector suite (Object/Entity/Player/Camera) to extend with a Stalker panel
- `CMakeLists.txt:53` — `GLOB_RECURSE ... CONFIGURE_DEPENDS "src/*.cpp"`, so **new files need no build-system edits**

### Decisions taken for this plan
- **Scope covers both tickets** — the perception layer *and* the Stalker. Stalker is blocked on perception, and perception does not exist, so a Stalker-only plan would not be testable.
- **Fix entity polymorphism properly**, then add a `Stalker : Entity` subclass.
- **Director ships as the pre-agreed fixed heuristic** now; full hint-budget Director is a follow-up.

---

## Blocker: entities cannot currently be subclassed

Two places destroy any `Entity` subclass. Both must be fixed before a `Stalker` type can exist.

**1. Spawn slices** — `src/gameLayer/gameMap.cpp:58`
```cpp
Entity* GameMap::saveEntity(Entity& entity) {
    ...
    auto entity_ptr = std::make_unique<Entity>(entity);   // slices any subclass
    scene->entities[entity.id] = std::move(entity_ptr);
```

**2. Load hardcodes the base type** — `src/gameLayer/SaveSystem.cpp:231`
```cpp
auto entity = std::make_unique<Entity>();   // a saved Stalker returns as a plain Entity
```

### Fix (Phase 0)
- Add to `Entity`: a virtual `virtual std::unique_ptr<Entity> clone() const` and an `EntityKind kind` tag serialized in `formatToJson()` / `loadFromJson()` (follow the existing `ObjectType`/`objectTypeToString` idiom in `GameObject.h`).
- Change `GameMap::saveEntity` to use `entity.clone()` instead of `make_unique<Entity>(entity)`.
- Add a small factory — `Entity::createByKind(EntityKind)` — and call it from `SaveSystem.cpp` in place of the hardcoded construction.
- Keep `saveEntity(Entity&)`'s signature so existing callers and the World Editor are unaffected.

Note the existing constructor comment in `Entity.cpp:24-31`: loaders deliberately **do not** call `onEnable()` (it would reset loaded health/stamina). Any Stalker state that must survive a load has to be installed in the constructor, not `onEnable()` — the same trap `InstallDefaultBuffs` already works around.

**Effort: ~4h.** This is the tax that unblocks every future creature, not just the Stalker.

---

## Phase 1 — Sound-only perception (Notion: `perception-emitters`, 16h)

New directory `src/gameLayer/Perception/` (repo convention is PascalCase under `gameLayer/`, not Notion's `src/perception/`).

**`SoundEvent.h`** — the seam between the two tickets:
```cpp
enum SoundKind { SOUND_FOOTSTEP, SOUND_STATION, SOUND_TAMPER, SOUND_IMPACT, SOUND_VOICE };

struct SoundEvent {
    Vector3   position;
    float     loudness;   // 0..1 at the source
    SoundKind kind;
    float     timestamp;
    uint64_t  sourceId;   // GameObject::id, so the Stalker can ignore its own noise
};
```

**`SoundField.h/.cpp`** — a scene-owned ring buffer of recent events (fixed capacity, no allocation per frame; `ArenaAllocator.h`/`PoolAllocator.h` exist if wanted).
- `Emit(SoundEvent)` — called by emitters
- `float AudibleLoudnessAt(Vector3 listener, const SoundEvent&) const` — inverse-square distance attenuation, then an occlusion multiplier from a single `RigidBody3D` raycast between source and listener
- Events expire after a short TTL so the Stalker cannot mine stale history

**Emitters to wire:**
| Source | Where | Loudness |
|---|---|---|
| Player footsteps | `Objects/Entity/Player.cpp` `update3D`, gated on `walkLerp`/`headTimer` | crouch ≈ 0.15, walk ≈ 0.5, sprint ≈ 1.0 |
| Task station minigame | `MiniGame.h` implementors / `Scene::SetMiniGame` | ~0.6, periodic while active |
| Tampering | task-station ticket | ~0.8, one-shot |
| Physics impacts | `RigidBody3D` collision resolution | scaled by impact velocity |

Emission stays **decoupled from FMOD** — `AudioManager` plays the audible sound, `SoundField` records the gameplay event. Coupling them would make perception depend on audio bank state.

**Effort: ~16h**, matching the Notion estimate.

---

## Phase 2 — Stalker FSM (Notion: `stalker-fsm`, part of 28h)

New directory `src/gameLayer/AI/`.

**`Stalker.h/.cpp`** — `struct Stalker : Entity`, `kind = ENTITY_STALKER`, overriding `update`, `render3D`, `onCollision`, `formatToJson`, `loadFromJson`, `clone`.

**4-state FSM, exactly as locked:**

| State | Behavior | Exits |
|---|---|---|
| `PATROL` | Follow authored waypoint loop at base speed | Any `SoundEvent` above hearing threshold → `INVESTIGATE` |
| `INVESTIGATE` | Move to the *heard position* (never the player's true position), slower, pausing | Louder/closer event → re-target; player contact → `HUNT`; arrive with nothing → `SEARCH_LAST_KNOWN` |
| `HUNT` | Pursue at sprint speed toward the most recent strong stimulus; `Attack()` on contact | Stimulus stale for N seconds → `SEARCH_LAST_KNOWN` |
| `SEARCH_LAST_KNOWN` | Sweep a radius around the last known position | Found → `HUNT`; timer expires → `PATROL` |

Design constraints to hold:
- The Stalker stores **`lastKnownPosition`, never a player pointer.** This is the single invariant that makes "sound only" true rather than aspirational. Enforce it by keeping `Scene::player` out of `Stalker`'s reachable state and asserting on it in debug.
- **Reuse the existing buff system** rather than adding new fields: `BUFF_HEARING` scales the detection threshold, `BUFF_SEARCH` scales `SEARCH_LAST_KNOWN` radius/duration. `getBuff()` and `CooldownTimer` already work.
- **Reuse `Entity::currentSpeed`** and its `isSprinting`/`isCrouching` modifiers instead of a parallel speed system — `Entity::update()` already computes it, so call `Entity::update()` from `Stalker::update()` first.

**Navigation — no navmesh, and building one is out of scope for one room.** Use a waypoint graph authored in the World Editor and serialized with the world (reusing the existing `formatToJson`/`loadFromJson` hooks), with straight-line steering between nodes and the existing `RigidBody3D` raycast for obstacle avoidance. This is the cheapest credible option for a single-room slice; a navmesh becomes a Phase 2 (content production) ticket if room count grows.

**Effort: ~16h.**

---

## Phase 3 — Director, fixed heuristic (part of 28h)

New `src/gameLayer/AI/Director.h/.cpp`, scene-owned, updated once per frame.

Per your decision, this ships as the **pre-agreed simplified form**: no hint budget or tension pacing yet.

- The Director may read **world facts only** — which station is active, how long since the Stalker last heard anything, how long the current state has run. It **must not read player position.**
- It emits at most one `DirectorHint { Vector3 region; float confidence; }` on a cooldown, biasing `PATROL` waypoint selection toward the region containing the active task station. That is a world fact, not player ground truth — which is exactly the distinction the locked decision protects.
- **Every hint is logged** (source fact → emitted region), because the integration ticket's acceptance criterion is *"Verify by logging what the Director passes."* Build the log channel now even though the heuristic is simple; it is the artifact that proves the invariant.

Structure `Director::Evaluate()` so the full version (hint budget, cooldowns, tension curve) replaces the heuristic body without changing the call site.

**Effort: ~4h.**

---

## Phase 4 — Verification tooling

There is **no test framework in the repo** and CI (`.github/workflows/cmake-multi-platform.yml`) only builds. Two cheap additions make the work verifiable:

1. **Stalker Inspector** — a new panel in `src/gameLayer/WorldEditorTools/`, matching the existing Entity/Player inspectors: current FSM state, time in state, last heard event (position/loudness/kind), `lastKnownPosition`, current waypoint, and the Director hint log. Add a debug draw for the audible-radius sphere and the path to the current target.
2. **FSM self-test harness** — a debug-only function that drives the FSM with synthetic `SoundEvent`s and asserts each locked transition, using the existing `src/platform/asserts.h`. Low cost, and it produces the "tests added" evidence tier that `veil-sync` requires before it will propose *Done*.

**Effort: ~4h.**

---

## Files touched

**New**
- `src/gameLayer/Perception/SoundEvent.h`, `SoundField.h/.cpp`
- `src/gameLayer/AI/Stalker.h/.cpp`, `StalkerState.h`, `Director.h/.cpp`
- `src/gameLayer/WorldEditorTools/StalkerInspector.cpp`

**Modified**
- `src/gameLayer/Entity.h/.cpp` — `EntityKind`, `clone()`, factory
- `src/gameLayer/gameMap.cpp:58` — `saveEntity` uses `clone()`
- `src/gameLayer/SaveSystem.cpp:231` — factory instead of hardcoded `Entity`
- `src/gameLayer/Scene.h` — owns `SoundField` + `Director`
- `src/gameLayer/Objects/Entity/Player.cpp` — footstep emission
- `src/gameLayer/WorldEditor.cpp` / `WorldEditorTools/ObjectBrowser.cpp` — spawn + inspect Stalker

No `CMakeLists.txt` change needed (glob is `CONFIGURE_DEPENDS`).

---

## Verification

```bash
cmake --preset linux-debug && cmake --build --preset linux-debug
```

Then in-game, against the integration ticket's acceptance criteria:
1. **Patrol** — spawn a Stalker, stand still crouched. It should loop waypoints and never approach you.
2. **Investigate** — sprint once, then crouch still. It must move to *where you were*, not where you are. This is the single most important observation: if it curves toward your new position, a ground-truth leak exists.
3. **Hunt** — keep making noise on approach; confirm sprint-speed pursuit and `Attack()` on contact.
4. **Search-last-known** — go silent mid-hunt. It must sweep the last known position, then return to patrol.
5. **Director** — read the hint log; confirm every entry traces to a world fact and none to player position.
6. **Save/load** — save with the Stalker mid-hunt, reload, confirm it returns as a `Stalker` (not a sliced `Entity`) with state intact. This is the Phase 0 regression test.
7. Run the FSM self-test harness in a debug build.

Log actual hours per phase — the Production Plan is explicit that the slice's whole purpose is producing that multiplier, and that *"slice closes with no measured hours"* is a named risk.

---

## Schedule

Notion has the Stalker at 28h, 25 Aug – 8 Sep, with perception at 16h, 17–24 Aug (both *Not started*; today is 16 Aug). This plan totals **~44h**, which fits both windows — but Phase 0 (~4h) is unbudgeted work that neither ticket accounts for. It comes out of the Stalker's 28h, leaving 24h for Phases 2–4 (~24h). That is a zero-slack fit.

The 8 Sep checkpoint says: *if Stalker FSM + Director is not Done, take the Room cut.* Given Phase 0 consumes the Stalker's slack on day one, taking the Room cut now (Option A in the Production Plan — Room 12h → 6h, buying 8h for integration) is worth deciding early rather than at the checkpoint. That is a Kuma decision, not one this plan takes.

---

## Tracker discrepancies found

Worth fixing separately — they will corrupt the next `veil-sync` run:

1. **The `veil-sync` skill points at a dead tracker.** It targets `collection://374c2671-520b-4f1f-8ef5-ff1a20dae0ca` (`Entity Implementation – Task Tracker`), which returns `data_source_not_found`. The live tracker is the **Tasks** database, `collection://ce12dd35-4540-45bd-a0c2-84c3c14a58dc`, with a different schema (Name/Status/System/Estimate/Milestone/Displaces, not Entity/Phase/Priority). The skill cannot run as written.
2. **`.veil-sync/LAST_SYNC` does not exist** — Step 1 of the Production Plan's git-linkage setup was never run, so sync silently falls back to a 7-day window.
3. **No commit template / `Veil-Task:` trailer** is configured (Step 2 never run).
4. **Notion's expected paths do not match the repo.** The plan expects `src/ai/`, `src/director/`, `src/perception/`; actual work will land in `src/gameLayer/AI/`, `src/gameLayer/Perception/`. The path heuristics in the sync will miss every commit unless updated — which makes the commit trailer the only reliable signal.
