# Lighting System — Development Plan

**Target:** Veil engine (C++23, raylib 6.0, OpenGL 3.3 core)
**Author:** Engine / Graphics
**Date:** 2026-07-30
**Status:** Approved for implementation

---

## Purpose and Scope

Veil currently renders every 3D object through raylib's **default unlit shader**. `GameObject::render3D()` calls
`DrawModel()` + `DrawModelWires()`, and raylib's `DrawMesh()` binds `material.shader` — which for every object in the
project is `rlGetShaderIdDefault()`. The result is flat, fully-bright albedo with no directional cue, no falloff, and
no occlusion. For a first-person horror title (**Subject Veil**), light *is* the primary gameplay and mood instrument,
so this is the highest-value renderer gap remaining.

This plan delivers a **forward-rendered, multi-light lighting system** with:

- Directional, point, and spot lights (8 simultaneous, hard shader limit).
- A single directional **shadow map** with 3×3 PCF filtering.
- Global atmosphere: ambient term, distance fog, exposure.
- A player **flashlight** that rides the active camera.
- Full editor authoring (Lighting Inspector) and world-file serialization.

Explicitly **out of scope** for this pass: deferred rendering, PBR/IBL, point-light cubemap shadows, cascaded shadow
maps, normal/roughness map sampling, bloom or any other post-process chain. Each of those is a separate milestone and
several of them require **a decision from the project owner about the target art direction** before they are worth
building.

---

## Step 0 — Render-Path Audit (must be first)

Nothing can be designed until the exact injection points are known. The audit produced five facts that every later
step depends on:

1. **`BeginShaderMode()` does not affect models.** It swaps rlgl's *batch* shader (used by `DrawGrid`, `DrawSphere`,
   `DrawBoundingBox`, all 2D shapes). `DrawMesh()` calls `rlEnableShader(material.shader.id)` itself. Therefore the
   lighting shader must be written **into every `Material`**, not pushed as a mode. This is the single most important
   finding in the audit.
2. **raylib auto-binds only some uniforms.** `LoadShaderFromMemory()` resolves `mvp`, `matView`, `matProjection`,
   `matModel`, `matNormal`, `colDiffuse`, `texture0/1/2` and the vertex attributes. It does **not** resolve
   `viewPos`; `shader.locs[SHADER_LOC_VECTOR_VIEW]` must be assigned by hand.
3. **`matNormal` is `transpose(inverse(matModel))`** (world space, see `rmodels.c:1535`), so non-uniform
   `rigidBody3D.scale` — which `render3D()` bakes into `model.transform` — is handled correctly for free.
4. **Asset models are shared by pointer.** `GameObject::loadVisuals()` does `model = modelAsset->model`; `Model::materials`
   is a raw pointer, so writing a shader into one object's material writes it into every object sharing that asset.
   That makes shader injection cheap and idempotent, but it also means **material state can never be used to store
   per-object data**.
5. **`LoadRenderTexture()` cannot be used for shadows.** Its depth buffer is a *renderbuffer* (`rlLoadTextureDepth(w, h, true)`)
   and is not sampleable. A depth-only FBO must be built directly with `rlLoadFramebuffer()` + `rlLoadTextureDepth(w, h, false)`
   + `rlFramebufferAttach(RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D)`.

**Importance:** skipping this step produces the classic failure where a lighting shader is loaded, `BeginShaderMode` is
called, and the scene renders identically — a bug that costs hours because nothing errors.

---

## Step 1 — Shader Asset Layout and Build Pipeline

Create `resources/shaders/glsl330/` holding `lighting.vs` and `lighting.fs`.

- `resources/` is already copied to the build output by a CMake `POST_BUILD` step, and `RESOURCES_PATH` resolves to the
  source tree in non-production builds — **no CMakeLists change is required**, which keeps the build surface untouched.
- `AssetManager::loadAll()` scans only `icons`, `blocks`, and `models`, and `isTextureFile()`/`isModelFile()` reject
  `.vs`/`.fs`, so the new folder cannot pollute the asset registry.
- The `glsl330` subfolder is deliberate: **if the project ever targets web (GLSL 100) or a GL 4.3 compute path, the
  version becomes a folder switch rather than a shader rewrite.**

**Importance:** doing this first means the shader files exist on disk before any C++ references them, so the first
compile-and-run already exercises the real load path instead of a stub.

---

## Step 2 — The Light Data Model

Define `Light` and `LightType` in `src/gameLayer/LightingSystem.h`.

```
enum LightType { LIGHT_DIRECTIONAL, LIGHT_POINT, LIGHT_SPOT };

struct Light {
    std::string name; LightType type; bool isEnabled; bool castsShadow;
    Vector3 position; Vector3 direction;
    Color color; float intensity; float range;
    float innerConeDegrees; float outerConeDegrees;
    Json formatToJson() const; bool loadFromJson(const Json&);
};
```

Design rules, each chosen against a concrete failure mode:

- `Color` (not `Vector3`) because **every other authored colour in the engine is a raylib `Color`** — `defaultColor`,
  the editor's `Vector4 colorHolder`, and the JSON `{r,g,b,a}` array format. Matching it keeps the Lighting Inspector
  and the save format consistent with `ObjectBrowser`.
- `intensity` is separate from `color` so an artist can push brightness past 1.0 without leaving the 0–255 colour space.
- Cone angles are stored in **degrees** (authoring units) and converted to cosines only at upload time; storing cosines
  would make the inspector unusable.
- `formatToJson`/`loadFromJson` mirror `GameObject`'s exact contract, so the save system needs no new patterns.

**Importance:** the data model is the contract shared by the shader uniforms, the editor panel, and the save format.
Changing it after those three exist means changing all three.

---

## Step 3 — LightingSystem Singleton Skeleton

`LightingSystem` follows the engine's established singleton shape (`AssetManager`, `AudioManager`, `Settings`,
`WorldEditor`): private constructor, deleted copy/move, `static getInstance()` with a function-local static.

Public surface:

| Group | Members |
|---|---|
| Lifecycle | `Init()`, `Shutdown()`, `IsReady()` |
| Frame | `Update(camera, delta)`, `RenderShadowPass(scene)`, `BindShadowMap()` |
| Injection | `ApplyToModel(Model&)`, `ApplyToLoadedAssets()`, `ApplyToScene(Scene*)` |
| Lights | `GetLights()`, `AddLight()`, `RemoveLight()`, `ClearLights()`, `CreateDefaultRig()` |
| Flashlight | `SetFlashlightEnabled()`, `ToggleFlashlight()`, `IsFlashlightEnabled()` |
| Atmosphere | public fields: `ambientColor`, `ambientStrength`, `fogColor`, `fogDensity`, `exposure`, … |
| Serialization | `formatToJson()`, `loadFromJson()`, `ApplySettings()` |

`Scene` is **forward-declared** in the header and only included in the `.cpp`. This matters: `GameObject.cpp` must
include `LightingSystem.h`, and `Scene.h` transitively includes `GameObject.h` — a real include cycle if the
forward declaration is skipped.

**Init ordering is a major engine change and must be exact.** In `init_game()`:

```
Settings::Init()  →  AudioManager  →  AssetManager::loadAll()  →  LightingSystem::Init()  →  SceneManager_init()
```

`SceneManager_init()` builds the Main Menu scene, which constructs `GameObject`s (each generating a cube model) and
calls `SaveSystem::LoadGame("mainMenu", …)`. If `LightingSystem::Init()` ran *after* that, every object created during
scene construction would keep the default unlit shader and the world would render half-lit with no error message.
`AssetManager::loadAll()` must still precede it so `ApplyToLoadedAssets()` has models to walk.

**Importance:** the skeleton and its init slot must exist before any rendering code, because every later step is a
method on this object.

---

## Step 4 — Forward Lighting Shader (GLSL 330)

`lighting.vs` passes world-space position and normal plus UV and vertex colour to the fragment stage, using only
uniforms raylib fills automatically (`mvp`, `matModel`, `matNormal`).

`lighting.fs` structure, in evaluation order:

1. **`depthPass` early-out.** When the shadow pass is rendering, the shader returns a constant immediately. This is not
   an optimisation — it is a **correctness requirement**. Without it, the fragment shader would sample `shadowMap` while
   the same texture is attached as the depth target, which is undefined behaviour in OpenGL.
2. Albedo = `texture0 × colDiffuse × fragColor`. `colDiffuse` carries the `tint` from `DrawModel(..., defaultColor)`,
   so per-object colour keeps working exactly as it does today.
3. Per-light loop, `break` once `i >= lightCount` so unused slots cost nothing.
4. **Art-directed falloff, not inverse-square:** `ratio = dist/range`, `att = (1 - ratio²)²`. A physical
   `1/d²` term would force intensity values in the hundreds and make `range` meaningless in the inspector. This
   formulation is exactly 1.0 at the source and exactly 0.0 at `range`, which is what a level designer needs.
5. Spot cone via `smoothstep(cos(outer), cos(inner), theta)` — note the argument order; `cos` decreases as the angle
   grows, so outer is the *lower* edge.
6. Blinn-Phong specular (`halfDir`), scaled by `specularStrength`.
7. Ambient, fog, exposure, then `clamp`.

**No gamma encode is applied, deliberately.** The engine loads PNGs as plain `UNCOMPRESSED_R8G8B8A8` with no sRGB
decode. Adding an output gamma without a matching input decode washes every surface out. **Moving Veil to a full linear
pipeline is a separate, project-wide decision** affecting texture loading, the 2D/UI pass, and every authored colour.

Two shader constants must be kept in lockstep with C++ by hand: `MAX_LIGHTS` (8) mirrors `LIGHT_LIMIT`, and the
shadow sampler's texture unit mirrors `SHADOW_MAP_SLOT`.

**Importance:** the shader defines the uniform names and semantics that Step 6 uploads. Writing the upload code first
guarantees a mismatch hunt.

---

## Step 5 — Material Injection

`ApplyToModel(Model&)` writes `lightShader` into `model.materials[i].shader` for all `materialCount` materials, and is a
no-op while `IsReady()` is false.

Call sites, and why each is required:

| Site | Covers |
|---|---|
| `LightingSystem::Init()` → `ApplyToLoadedAssets()` | every shared `AssetManager` model |
| `GameObject::GameObject()` | the fallback unit cube built in the constructor |
| `GameObject::loadVisuals()` | every rebind — placement, duplication, `loadCommonFromJson`, `setModel`, `onEnable` |

`loadVisuals()` is the important one: it is the funnel through which *all* runtime model binding passes, including save
loading and editor placement. Covering it means new objects are lit automatically forever.

**Importance:** this is the step that actually makes lighting visible. Steps 1–4 produce zero visual change on their own.

---

## Step 6 — Per-Frame Uniform Upload

`Update(camera, delta)` runs once per frame from `SceneManager_draw()`, before any 3D drawing:

1. Drive the flashlight from `camera.position` / `camera.target`.
2. Pack enabled lights into flat `float`/`int` arrays. **The flashlight is packed first** so it can never be evicted by
   eight authored world lights — losing the player's own light source to an array overflow would be a gameplay bug, not
   a visual one.
3. Guard every normalise against a zero-length `direction` (an inspector field the user can zero out), and force
   `cos(inner) > cos(outer)` so the spot `smoothstep` can never divide by zero.
4. Upload with `SetShaderValueV(..., LIGHT_LIMIT)` — always the full array, zero-filled tail, so a stale slot from a
   previous frame can never leak in.
5. Upload `viewPos` through the hand-assigned `SHADER_LOC_VECTOR_VIEW`, then atmosphere and shadow tuning.

**Importance:** must follow Step 4 (names) and precede Step 7 (which reuses the same uniform block for shadow state).

---

## Step 7 — Shadow Mapping

The most failure-prone step; built in this internal order:

1. **Depth-only FBO** via rlgl (Step 0, finding 5). A fake `RenderTexture2D` is populated by hand — `.id` from
   `rlLoadFramebuffer()`, `.depth` from `rlLoadTextureDepth(res, res, false)`, `.texture.width/height` set so
   `BeginTextureMode()` reports the correct FBO size and `BeginMode3D()` computes an aspect ratio of 1.0. Verified with
   `rlFramebufferComplete()`.
2. **Light camera.** Orthographic, `fovy = shadowDistance`, aimed along the shadow light's direction and **centred on
   the player** so the finite map follows the play area. `up` flips to `(0,0,1)` when the light is near-vertical,
   otherwise `MatrixLookAt` degenerates.
3. **Caster pass.** A dedicated `DrawShadowCasters()` — *not* `Scene_drawScene3D()`. The scene draw calls
   `scene->draw3D()` (which runs `DrawGrid`), `DrawModelWires`, `DrawBoundingBox`, and the direction-debug spheres, all
   of which use the rlgl batch shader and would write depth into the shadow map, stamping grid lines and debug gizmos
   into the world as real shadows.
4. **`lightVP` capture** as `MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection())` from inside
   `BeginMode3D(lightCamera)`, matching raylib's row-vector convention.
5. **Sampler binding at texture slot 15.** raylib's `DrawMesh()` binds material maps to slots `0 … MAX_MATERIAL_MAPS-1`
   (12) and unbinds them afterwards, so any slot below 12 can be clobbered mid-frame. Slot 15 is the top of the GL 3.3
   guaranteed minimum of 16 units.
6. **Slope-scaled bias + 3×3 PCF** in the shader, plus an out-of-frustum test that returns *lit* rather than shadowed —
   otherwise everything beyond `shadowDistance` renders pitch black.

A new **`GameObject::castsShadow` flag** is required for this step. It defaults to `true` and is serialised with a
`true` default, so old save files load unchanged. `Scene_new()` sets `player->artifact->castsShadow = false`: the
artifact floats one unit in front of the camera, and left enabled it smears a full-screen shadow over the view.

**Importance:** shadows depend on the shader (`depthPass`, `lightVP`, `shadowMap`), on injection (casters need the
lighting shader bound), and on the caster flag. It cannot move earlier.

---

## Step 8 — Atmosphere: Ambient, Fog, Exposure

Ambient (`colour × strength`) keeps unlit surfaces readable instead of pure black. Fog is
`1 - exp(-(dist × density)²)` — exponential-squared, which stays transparent up close and closes off hard at range,
the sightline control a horror game actually wants.

This step includes one **change to engine-wide behaviour**: `update_game()` currently calls `ClearBackground(WHITE)`.
Nothing draws a sky, so the clear colour *is* the horizon. With fog enabled against a white clear, geometry fades into
a bright wall. The clear now uses the lighting system's fog colour, so distance dissolves seamlessly into the
background. **This visibly changes every existing scene** and is called out here rather than buried in the diff.

**Importance:** atmosphere reuses the uniform upload from Step 6 and needs the fog distance the shader already
computes for shadows; folding it in earlier would mean two passes over the same code.

---

## Step 9 — Player Flashlight

A spot light owned by `LightingSystem` and stored **outside** the `lights` vector, so it can never be deleted in the
inspector or written into a world file.

This requires a **new engine input action, `ACTION_USE_FLASHLIGHT`** (default `KEY_L`, gamepad
`GAMEPAD_BUTTON_RIGHT_FACE_UP`), added to `ActionType` and `SetDefaultActions()`. Inserting into the middle of the enum
is safe *today* because keybinds are not yet persisted (`InputSystem::InputSystem()` has a `// Load From File else Set
Default` comment with no loader). **Once keybind serialisation lands, appending becomes mandatory** — noted here so the
future change is not a silent breakage.

The toggle is read in `UpdateActions()` in `Player.cpp`, alongside the other gameplay actions, and is therefore
suppressed while the World Editor is active — consistent with fire/interact/jump.

**Importance:** the flashlight is the gameplay payoff of the whole system, but it is only a data producer for Step 6's
packer. Building it before the packer means writing it twice.

---

## Step 10 — Editor Tooling: Lighting Inspector

`src/gameLayer/WorldEditorTools/LightingInspector.cpp` implements `WorldEditor::ShowLightingData()`, following the
existing panel pattern exactly: `ImGui::Begin`, magenta `TextColored` section headers, `PushID`/`PopID` per list entry,
and a `statusMessage` write-back.

Wiring in `WorldEditor`: an `isLightingActive` flag, a `Ctrl+8` shortcut, a hub checkbox, and a dispatch line — the
same four edits every existing panel needed.

Panel contents: shader/shadow diagnostics, atmosphere sliders, shadow tuning with a resolution selector that
reallocates the FBO, the light list with add/duplicate/delete, a full per-light editor, "Add Light At Camera", and the
flashlight toggle.

**Importance:** deliberately *after* the runtime. A panel written against an unfinished data model is rework, and
until Step 7 works there is nothing meaningful to tune.

---

## Step 11 — Serialization

Lights are level data and must survive a save/load round trip.

- `SaveSystem::VERSION` **2 → 3**. The loader's guard is `version > VERSION`, so v2 files keep loading.
- `SceneToJson()` and `SaveWorld()` both write `j["Lighting"]`; `ApplyJsonToScene()` and `LoadWorld()` both read it.
- **Missing `"Lighting"` key → `CreateDefaultRig()`**, not an empty light list. Every existing save file
  (`mainMenu.json`, `world.json`, `backup.json`) predates this system; defaulting to zero lights would open the project
  to a black screen and read as a broken build.

**Importance:** must follow Step 2 (the JSON shape) and Step 10 (so authored lights have somewhere to persist to).

---

## Step 12 — Settings Integration and Quality Scaling

`Settings` already declares `shadows`, `luminosity`, `postProcessing`, and `effects` with no consumers.
`ApplySettings()` gives two of them meaning:

- `shadows` (0–4): `0` disables shadows; `1–4` select 512 / 1024 / 2048 / 4096 map resolutions and PCF softness.
- `luminosity` drives `exposure`, with a `0 → 1.0` fallback because a freshly written `settings.json` stores `0` and
  would otherwise render an entirely black frame.

Called from `Init()` **before** the FBO is allocated, so the map is created at the right size the first time.

**Importance:** requires the shadow map to exist as a resizable resource (Step 7) and the exposure uniform (Step 8).

---

## Step 13 — Shutdown and Resource Ownership

`Shutdown()` unloads the shader and the FBO. Ownership rules:

- `rlUnloadFramebuffer()` queries the depth attachment and deletes the texture itself (`rlgl.h:3929`). Calling
  `UnloadTexture(shadowMap.depth)` as well is a **double free**.
- Materials hold a *copy* of the `Shader` struct, not ownership. After `Shutdown()` those copies dangle, so shutdown
  must be the last engine call — `close_game()` in `ProjectVeil.cpp` runs before `CloseWindow()`, while the GL context
  is still alive, which is the correct slot.

**Importance:** last, because it must unwind everything the previous steps allocated.

---

## Step 14 — Bug Review Pass

Reviewed against these specific traps, all found during the audit rather than after:

| Risk | Resolution |
|---|---|
| Shader sampling its own depth target | `depthPass` early-out (Step 4/7) |
| Shadow sampler on a slot raylib clobbers | slot 15, above `MAX_MATERIAL_MAPS` (Step 7) |
| `rlUnloadFramebuffer` + `UnloadTexture` double free | FBO unload only (Step 13) |
| `Init()` after scene construction → half-lit world | fixed init order (Step 3) |
| Zero-length `direction` from the inspector | guarded normalise (Step 6) |
| `inner == outer` cone → divide by zero | forced `cosInner > cosOuter` (Step 6) |
| Grid/wireframe/debug gizmos casting shadows | dedicated caster pass (Step 7) |
| Old saves with no lights → black screen | `CreateDefaultRig()` fallback (Step 11) |
| `luminosity == 0` from a fresh settings file → black screen | `0 → 1.0` fallback (Step 12) |
| Out-of-frustum fragments read as shadowed | early `return 1.0` (Step 7) |
| Include cycle `LightingSystem ↔ Scene` | forward declaration (Step 3) |
| Stale light data in unused array slots | always upload the full array (Step 6) |

Verification: build under the VS 18 Insiders dev shell, then a headless run with log capture to confirm shader load,
FBO completeness, asset injection, and a clean shutdown.

---

## Step 15 — Documentation and Release Notes

`Release.md` (new, repo root) records the feature set, the authoring workflow, tuning defaults, save-format change, and
known limitations. `README.md` gains a Lighting System entry under *Implemented*. `Changelog.txt` gains
Added/Updated entries in its existing bracket format.

**Importance:** last, so it documents what shipped rather than what was planned.

---

## Ordering Review — Why This Sequence

Each row states what the step depends on and what breaks if it is moved earlier.

| # | Step | Depends on | If moved earlier |
|---|---|---|---|
| 0 | Render-path audit | — | `BeginShaderMode` used instead of material injection; silent no-op |
| 1 | Shader assets | 0 | C++ loads a path that does not exist |
| 2 | Light data model | 0 | uniform names, editor, and save format diverge |
| 3 | Singleton skeleton | 2 | no owner for shader/FBO handles; init-order bug ships |
| 4 | Lighting shader | 1, 2 | upload code written against guessed uniform names |
| 5 | Material injection | 3, 4 | shader compiles but nothing on screen changes |
| 6 | Uniform upload | 4, 5 | lights exist in C++ but never reach the GPU |
| 7 | Shadow mapping | 4, 5, 6 | depth feedback loop, wrong sampler slot, debug-gizmo shadows |
| 8 | Atmosphere | 6, 7 | fog fights a white clear colour; second pass over upload code |
| 9 | Flashlight | 6 | written twice — once standalone, once against the packer |
| 10 | Lighting Inspector | 2, 6, 7, 8 | panel built against an unfinished model; nothing to tune |
| 11 | Serialization | 2, 10 | authored lights lost on reload |
| 12 | Settings | 7, 8 | quality toggles with no resource to resize |
| 13 | Shutdown | 3, 7 | leaks or double-frees whatever exists at the time |
| 14 | Bug review | 1–13 | reviews an incomplete system |
| 15 | Documentation | 14 | documents intent instead of behaviour |

**Critical path:** 0 → 4 → 5 → 6 → 7. Steps 9, 10, 11, and 12 are leaves off that spine and could be reordered among
themselves; 0, 4, 5, 6, and 7 cannot move without introducing one of the failures listed in Step 14.

---

## Deferred — Requires a Project Decision

These are ready to build but each needs **direction from the project owner** before it is worth the render budget:

- **Point/spot shadows** (cubemap or per-light 2D atlas) — a large VRAM and draw-call commitment.
- **Cascaded shadow maps** — only if the target play space exceeds the single map's useful `shadowDistance`.
- **Linear/sRGB pipeline + tonemapping** — engine-wide; changes every authored colour in every existing save file.
- **Normal and roughness map sampling** — depends on whether the AmbientCG texture sets are re-exported with their
  full map complement (only `_Color` is currently vendored).
- **Light volume culling** — needed only once authored light counts routinely exceed the 8-light limit.
