# Release Notes — Week of 2026-09-06 to 2026-09-13

**Engine:** Veil
**Game:** Subject Veil
**Branch covered:** `main`

---

## Summary

This week's work centers on the collision system: `Collider3D` now owns both collision detection and response,
replacing a set of scattered checks spread across `RigidBody3D` and `GameObject`. That refactor exposed (and this
week also fixed) a nasty timing bug that had been quietly running the entire game at half speed, plus a spawn-recovery
bug that left fallen players stuck in the void forever instead of being recovered. A new landing sound and positional
3D audio round out the player-facing changes.

---

## New

- **Player landing sound.** Jumping (or falling) and hitting the ground now plays a dedicated landing sound, triggered
  directly off `RigidBody3D`'s landing event rather than being bolted on elsewhere. It's the first sound tied to
  physics state rather than to a direct player action.

## Improved

- **Collision now lives in one place.** `Collider3D` is the single owner of both collision detection and collision
  response; the old `RigidBody3D::canCollide` and `GameObject::onCollision` call paths are gone. This also moved
  `Collider.h` from the engine layer into the game layer, where the rest of gameplay collision code already lived.
- **Stalker AI uses the engine's real line-of-sight and state-transition checks** instead of a hand-rolled raycast,
  and gained a proper `CheckIfTargetIsRadius` target-radius check tied into the new collision system. Behavior should
  be more consistent with how every other system judges visibility and proximity.
- **3D audio is positional.** `AudioManager` now plays sounds from the emitting object's actual world position instead
  of a flat, non-positional mix, and it discovers audio files and sound banks via a recursive directory search instead
  of a fixed, shallow file list — new audio assets no longer need to be registered by hand.
- **World Settings correctly displays DeltaTime** as a string instead of a raw/garbled value, and the game's minimum
  and maximum framerate bounds were retuned.
- Removed a large amount of dead and duplicated collision code left over from the `Collider3D` refactor (an empty,
  uncalled `Collider` stub class, unused includes, and leftover fields), reducing the risk of someone editing the
  wrong copy of collision logic by mistake.

## Fixed

- **Game was accidentally running at half speed.** A frame-time cap introduced earlier this week to guard against
  long hitches used a threshold (1/120s) that was *smaller* than the game's normal 60 FPS frame time, so it clamped
  `deltaTime` on essentially every frame instead of only during a real stall — quietly halving the speed of all
  frame-rate-independent gameplay (movement, physics, animations, minigame timers) at the shipped target framerate.
  The cap has been corrected to only trigger below a 15 FPS floor, where it belongs.
- **Falling off the map no longer strands the player.** `Scene::clampObject` logged a "recovering to spawn point"
  message whenever a `GameObject`, `Entity`, `Player`, or `InteractableObject` fell below y = -1000, but the actual
  teleport-to-spawn logic had been left commented out, so only velocity was reset — the object just stopped falling
  and hung there permanently. Recovery now actually teleports the object back to its own recorded spawn point (or a
  safe default if it doesn't have one).
- **Player spawn location is now explicitly defined** in `Scene.cpp` rather than relying on incidental defaults.
- Two latent crash risks were closed proactively: `Scene`'s `update`/`draw2D`/`draw3D` function pointers are now
  defaulted to `nullptr` and null-checked before use (previously uninitialized), and `Player::render3D()` now calls
  through `Entity::render3D()` instead of skipping straight to `GameObject::render3D()`, so future entity-specific
  rendering logic won't silently be bypassed for the player.

## Breaking

No player-facing breaking changes this week. Internally, `GameMap`'s old map-wide `spawnPoint` / `hasSpawnPoint`
fields were removed in favor of per-`Entity` spawn points as part of the spawn-recovery fix — this only affects
code that directly read those fields, not saved worlds or player-facing behavior.

---

*Compiled from commits merged to `main` between 2026-09-06 and 2026-09-13.*
