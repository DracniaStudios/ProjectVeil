# Veil

**Veil** is a custom C++23 game engine and runtime built from scratch on top of [raylib](https://www.raylib.com/), developed as the foundation for the in-development title **Subject Veil**. It provides the entity/object model, physics, scene management, save system, and tooling needed to build and iterate on a first-person 3D game, along with a suite of in-editor developer tools for inspecting and tuning the game live.

> Veil is under active, early development. APIs, systems, and structure are still evolving.

---

## Table of Contents

- [Overview](#overview)
- [Tech Stack](#tech-stack)
- [Systems](#systems)
  - [Implemented](#implemented)
  - [Planned](#planned)
- [Subject Veil — The Game](#subject-veil--the-game)
- [Project Structure](#project-structure)
- [Building](#building)

---

## Overview

Veil is organized as two layers:

- **`src/platform`** — the platform entry point and low-level bootstrapping (window/context creation, asserts).
- **`src/gameLayer`** — the engine and game logic: entities, objects, scenes, physics, assets, saves, mini-games, and the in-game developer tools.

The engine favors a lightweight, data-oriented `GameObject` / `Entity` model over a full ECS, with scenes owning their own map data, physics bodies, and interactable objects. It is designed to support both 3D exploration and 2D mini-game contexts within the same scene.

## Tech Stack

| Component | Library |
|---|---|
| Language | C++23 |
| Build System | CMake (with presets for Windows, Linux, and macOS) |
| Rendering / Windowing | [raylib 6.0](https://www.raylib.com/) (OpenGL 3.3 core, GLSL 330) |
| Editor UI | [Dear ImGui (docking)](https://github.com/ocornut/imgui) via [rlImgui](https://github.com/raylib-extras/rlImGui) |
| Procedural Noise | [FastNoise2](https://github.com/Auburn/FastNoise2) |
| Serialization | [nlohmann/json](https://github.com/nlohmann/json) |
| Audio | [FMOD](https://www.fmod.com/) |

CI builds are run via GitHub Actions across Linux and Windows using both GCC/Clang and MSVC toolchains.

## Systems

### Implemented

- **Entity / GameObject Model** — a shared base (`GameObject`) for anything that exists in the world, with `Entity` and `InteractableObject` specializations, JSON (de)serialization hooks, lifetime/decay handling, and per-object render/update/collision callbacks.
- **Scene & Map Management** — `SceneManager` and `Scene` own the active `GameMap`, player, camera, entities, and interactables, and support switching between 3D world mode and 2D mini-game mode.
- **Lighting System** — forward-rendered lighting with up to 8 simultaneous directional/point/spot lights, directional shadow mapping (depth-only FBO with 3×3 PCF and slope-scaled bias), ambient and exponential distance fog, exposure, a camera-tracking player flashlight, and a live Lighting Inspector with in-world gizmos showing each light's position, direction, and influence volume. Lights are authored in-editor and serialized with the world. See [Release.md](Release.md) and [docs/LightingSystemPlan.md](docs/LightingSystemPlan.md).
- **Physics** — custom 2D and 3D rigid body implementations (`RigidBody2D` / `RigidBody3D`) with gravity, drag, constraint resolution, and raycast-based collision checks. 3D collision is a two-phase solver: an axis-aligned broad phase gating an exact oriented-box narrow phase (15-axis separating axis test). Bodies carry a **`Collider3D`** — a collision volume sized and offset independently of the render scale, as a Box, a Sphere, or a Mesh box auto-fitted to the loaded model's bounds — in either **Collision** mode (resolves through the rigid body) or **Trigger** mode (reports the overlap and applies no physics). See [docs/ColliderSystemPlan.md](docs/ColliderSystemPlan.md).
- **Asset Manager** — a singleton responsible for loading and looking up textures/materials by name, backing all world and UI rendering.
- **Save System** — JSON-backed serialization for game objects, entities, and world state, with save/load of full scenes to disk.
- **Inventory & Items** — a slot-based inventory with stackable items (starting with a set of health potions) and item-to-texture resolution.
- **Interactable Objects** — world objects the player can interact with, including lockable containers (`LockedBox`) and objects that launch mini-games.
- **Mini-Game Framework** — a shared `MiniGame` interface (update/draw/data) driving self-contained arcade-style mini-games:
  - Flappy Bird
  - Crane (claw machine)
  - Doctor
  - Simon Says
  - Timed Simon Says
  - Maze
  - Ro-Sham-Boo (Rock, Paper, Scissors)
- **Particle System** — a lightweight particle emitter for world/UI effects.
- **Developer Tools** — an in-game ImGui-based developer window with dedicated inspectors:
  - Object Inspector
  - Entity Inspector
  - Player Inspector
  - Camera Inspector
  - Mini-Game Inspector
  - Asset Inspector
  - Game (world) Inspector
- **Memory Utilities** — arena and pool allocators for hot paths that don't need general-purpose heap allocation.
- **Audio Integration** — FMOD wired in for sound playback.

### Planned

Tracked informally via `Changelog.txt` as the project moves toward its next milestones:

- Hardened texture/asset loading (fixing null texture loads on certain objects).
- Reliable FMOD file detection through the audio interface.
- Timed (non-immediate) removal/decay of game objects.
- Expanded save system coverage (assets, inventory, and mini-game state alongside world/object data).
- Continued build-out of the developer tooling suite as new systems are added.

## Subject Veil — The Game

**Subject Veil** is the first-person 3D game built on top of the Veil engine. It combines free-form 3D exploration with interactable objects and puzzle containers scattered through the world, some of which drop the player into short, self-contained arcade mini-games (claw machines, timed memory tests, reflex games, and more) before returning them to the main scene. The game is still early in development — current builds focus on proving out the engine's core systems (movement, physics, interaction, saving/loading, and the mini-game framework) rather than final content or narrative.

## Project Structure

```
ProjectVeil/
├── src/
│   ├── platform/         # Entry point, platform bootstrapping, asserts
│   └── gameLayer/
│       ├── Components/       # RigidBody2D/3D implementations
│       ├── DeveloperTools/   # ImGui inspector windows
│       ├── MiniGames/        # Mini-game implementations
│       ├── Objects/
│       │   └── Interactable/ # Interactable world objects (e.g. LockedBox)
│       ├── Utility/          # Arena/Pool allocators
│       └── scenes/           # Scene constructors (e.g. Main Menu)
├── docs/                 # Development plans (e.g. LightingSystemPlan.md)
├── resources/            # Textures, models, icons, and shaders
│   └── shaders/glsl330/  # Forward lighting vertex/fragment shaders
├── saves/                # Serialized world save data
└── thirdparty/           # Vendored dependencies (raylib, imgui, FastNoise2, json, FMOD, rlImgui)
```

## Building

Veil uses CMake with presets for Windows (MSVC), Linux, and macOS.

```bash
# Configure (pick the preset matching your platform)
cmake --preset linux-debug   # or macos-debug, x64-debug, x86-debug

# Build
cmake --build --preset linux-debug
```

Resources are copied to the build output directory automatically as a post-build step, and the `RESOURCES_PATH` macro points to the source `resources/` directory in non-production builds.
