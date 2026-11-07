# Release Notes — Week of 2026-10-31 → 2026-11-07

**Game:** Subject Veil (Engine: Veil) · **Base version:** 0.4.24
**Range:** `2a0665e` (Oct 31) → `919d044` (Nov 6), 28 commits

## Summary

This week was the multiplayer foundation week. The game now simulates at a fixed 60 Hz, identifies players by Slot instead
of assuming a single local player, and single-player runs through the same server/client path that multiplayer uses. On
top of that sit a network transport, a host-and-join flow, and persistent world state. The Stalker also got a jumpscare
and a smarter chase, and several crash and warning fixes landed.

---

## New

### Jumpscare when the Stalker catches you
When the Stalker attacks, a new Jumpscare overlay now plays, and the Pause overlay and jumpscare are both handled by the
overlay stack. Previously a catch had no dedicated feedback. This makes the failure moment readable and sets up the
fail/win round flow below.

### Host and Join multiplayer
A Host can now start a server in its own process and play on it as Slot 0, and other players can join by address. The
transport is built on GameNetworkingSockets (`GnsTransport`) behind a transport-agnostic `INetTransport` interface with
protocol v1. There is also a dev network window with capsule stand-ins for remote players and a faked-network option for
testing latency and loss. This is early, dev-facing functionality, not a polished lobby.

### Single-player runs through the server path
`ServerWorld` and `ClientWorld` now exist, and single-player is played through an in-process `LoopbackTransport`. You
should see no difference, but single-player and multiplayer now share the same simulation code. Task stations are owned
by the server, so their state can't diverge between players.

### Persistent world state (SpacetimeDB)
A Host's world state, meaning which Task stations are completed, can be stored in a SpacetimeDB database through the new
`veil_world` module and `SpacetimeWorldStore`. Single-player keeps using World save files through `FileWorldStore`. A
`host.bat` script starts SpacetimeDB and publishes `veil_world` on the Host PC in one step.

### Fail and win states, per player
Rounds now have explicit fail and win states, and catches are tracked per Slot. A catch triggers a Full wipe: every
player returns to spawn, world state and Director memory are cleared, and the Stalker returns to Patrol. Objects keep
their ids, so this is not the same as clearing the scene.

### Stalker audio warnings and station sound
The Stalker now gives audible warnings as it changes state, handled by a new `StalkerWarnings` client module. Activating
a station plays a dedicated activation sound. Both are played from the client side so every player hears them.

---

## Improved

### Fixed 60 Hz simulation and a 4-state Stalker
The simulation ticks at a fixed 60 Hz, independent of frame rate, and player input is captured as a `PlayerCommand`
sampled by a `CommandSampler`. The Stalker's FSM was reduced to four states and gains an "unreachable" fallback for when
it can't path to a target. The Stalker now transitions to Hunt after Chase.

### Players identified by Slot
Editor panels, lighting, audio and saving no longer assume one local player; they look players up by Slot. Nothing
changes in single-player, but this was required for more than one player to exist in a world.

### GameMap moved onto EnTT
`gameMap` now stores its data in EnTT, a hybrid design kept deliberately close to the old interface. Flecs was
evaluated and removed from the build. The ECS and multiplayer libraries (EnTT, Serialize, GameNetworkingSockets, Steam)
were vendored this week, and the unused FastNoise2 NodeEditor tools were dropped from `thirdparty`.

### Asset and save paths resolved reliably
A new `ResourcePaths` module resolves resource and save locations, fixing cases where saves and resources were looked up
relative to the wrong directory. Asset loading, audio, lighting, settings and the save system all use it now.

### Large assets moved to Git LFS
Audio banks, music, textures and material files are now stored with Git LFS. Contributors need Git LFS installed
(`git lfs install`, then `git lfs pull`) or resources will appear as small pointer files.

### Test coverage
New test suites cover the network protocol, GNS transport, SpacetimeDB world store, Stalker FSM, render-off ticking and
ECS-backed GameMap. All run through `tests/run_tests.sh`.

---

## Fixed

### Player sound emission used the wrong direction
The player's sound-emit function was driven by Velocity instead of Move Direction. Emitted noise now follows where the
player is trying to move, which makes Stalker perception more predictable. The related emitter tests were cleaned up.

### Stalker KILL contact handling restored
An earlier scan change had altered contact handling, so catches could fail to register. KILL contact is handled again,
and `render3D` now null-checks the scene to avoid a crash when no scene is loaded. The leftover STALKER_KILL AI case was
removed.

### Compiler warnings cleaned up
Unused-parameter warnings in the camera and mini-game inspectors, Stalker and GameObject were silenced, along with unused
variables in the emitter tests. No behavior changes.

---

## Breaking

### Git LFS is now required
A fresh clone without Git LFS will build with placeholder resource files. See "Large assets moved to Git LFS" above.

### Build and dependency changes
The build now requires the newly vendored libraries (EnTT, Serialize, GameNetworkingSockets). Multiplayer persistence
additionally needs SpacetimeDB on the Host PC (`host.bat`). Flecs and the FastNoise2 NodeEditor tools are no longer
part of the build.

### Network protocol v1 and saves
Protocol v1 is new and not yet frozen; hosts and clients must run the same build. Anything that assumed a single local
player (custom tools, scripts) should use Slot-based lookups.
