# Release Notes — Lighting System

**Engine:** Veil
**Game:** Subject Veil
**Version:** 0.3.0
**Date:** 2026-07-30
**Plan:** [docs/LightingSystemPlan.md](docs/LightingSystemPlan.md)

---

## Summary

Veil now has a real lighting pipeline. Before this release every 3D object was drawn with raylib's default unlit
shader — flat, fully-bright albedo with no directional cue, no falloff and no occlusion. The engine now forward-renders
up to eight simultaneous lights per pass with shadow mapping, distance fog and a player flashlight, all authored live
from a new editor panel and persisted in world files.

For a first-person horror title this is a systems change, not a cosmetic one: darkness is now something a level can be
built around.

---

## Added to the Lighting System

### Lights

- **Three light types** — directional (sun/moon, no falloff), point (omnidirectional), spot (cone).
- **Eight simultaneous lights**, uploaded as flat uniform arrays in one forward pass.
- **Art-directed attenuation.** Point and spot lights use `(1 - (d/range)²)²`: exactly full brightness at the source
  and exactly zero at `range`. Chosen over physical inverse-square so `range` is a number a level designer can reason
  about instead of a falloff constant needing intensity values in the hundreds.
- **Smooth spot cones** with independent inner/outer angles authored in degrees.
- **Per-light colour and intensity kept separate**, so brightness can exceed 1.0 without leaving the 0–255 colour space.
- **Blinn-Phong specular** with global strength and shininess controls.

### Shadows

- **Directional shadow mapping** through a hand-built depth-only framebuffer. raylib's `LoadRenderTexture()` attaches
  depth as a renderbuffer, which cannot be sampled, so the target is assembled from `rlLoadFramebuffer()` +
  `rlLoadTextureDepth(res, res, false)`.
- **3×3 PCF filtering** with an adjustable tap radius, softening the shadow map's hard texel staircase.
- **Slope-scaled depth bias**, so surfaces seen edge-on by the light do not self-shadow.
- **A player-centred light camera**, so the finite map follows the play area rather than the world origin.
- **Selectable resolution** — 512 / 1024 / 2048 / 4096, reallocated live.
- **A dedicated shadow-caster pass** that draws geometry only. Reusing the normal scene draw would have written the
  debug grid, wireframes, bounding boxes and direction gizmos into the depth map as real world shadows.
- **Per-object `castsShadow` opt-out** on `GameObject`, serialised with a `true` default so existing worlds are unchanged.

### Atmosphere

- **Ambient term** (colour + strength) so unlit surfaces stay readable rather than pure black.
- **Exponential-squared distance fog** — clear up close, closing off hard at range.
- **Exposure control**, wired to the existing `Luminosity` display setting.

### Player

- **Flashlight** (`L`, or gamepad D-pad up) — a spot light that tracks the active camera, with its own colour,
  intensity, range and cone controls. It is owned by the lighting system rather than stored in the light list, so it
  can never be deleted in the editor or written into a world file, and it is packed into the shader first so eight
  authored world lights can never evict it.

### Tooling

- **New Lighting Inspector** (`Ctrl+8` inside the World Editor, `F1`):
  - Live diagnostics — shader id, active light count, shadow map size, current shadow caster.
  - Atmosphere: ambient colour/strength, fog colour/density, exposure, specular strength, shininess.
  - Shadows: enable, resolution, bias, distance, softness, and reload-from-settings.
  - Flashlight: toggle plus full beam tuning.
  - Light list with add-at-camera, add-directional, duplicate, delete, and reset-to-default-rig.
  - Per-light editor with a type switch that hides the fields that do not apply (position on a directional light,
    direction on a point light, cone on anything but a spot).
  - "Casts Shadow" is exclusive — enabling it on one light clears it everywhere else, since there is one shadow map.
  - "Reapply Shader To Scene" as a diagnostic safety net.

---

## Changed in the Project

These are engine-wide changes, not additions:

- **Every 3D material now carries the lighting shader.** raylib's `DrawMesh()` reads `material.shader`, so
  `BeginShaderMode()` cannot light models — it only affects the rlgl batch. The shader is injected in
  `GameObject::loadVisuals()` (the funnel for all runtime model binding), in the `GameObject` constructor, and across
  every shared `AssetManager` model at startup.
- **The backbuffer clear colour is now the fog colour**, not `WHITE`. Nothing draws a sky, so the clear colour *is*
  the horizon; against white, fogged geometry faded into a bright wall. This visibly changes every existing scene.
- **Engine start-up order is now fixed**: `Settings → Audio → AssetManager → LightingSystem → SceneManager`.
  `SceneManager_init()` constructs the Main Menu scene and loads its save file, so lighting has to be up first or
  those objects keep the unlit shader — a failure with no error message anywhere.
- **New input action `ACTION_USE_FLASHLIGHT`** (`KEY_L`, `GAMEPAD_BUTTON_RIGHT_FACE_UP`). Inserted mid-enum, which is
  safe only because keybinds are not yet persisted. Once keybind serialisation lands, new actions must be appended.
- **Save format version 2 → 3.** Adds a `"Lighting"` section to both full saves and world files, plus `"CastsShadow"`
  on every object. The loader only rejects files newer than the current version, so version 2 saves still load — and a
  save with no `"Lighting"` section installs the default lighting rig rather than opening a black level.
- **Two dormant Settings entries now do something**: `Shadows` (0 = off, 1–4 = 512/1024/2048/4096 plus PCF softness)
  and `Luminosity` (exposure, with a `0 → 1.0` fallback because a freshly written `settings.json` stores 0 and would
  otherwise render an entirely black frame).

---

## New Files

| File | Purpose |
|---|---|
| `resources/shaders/glsl330/lighting.vs` | Vertex stage — world-space position and normal |
| `resources/shaders/glsl330/lighting.fs` | Fragment stage — lights, shadows, fog, exposure |
| `src/gameLayer/LightingSystem.h` / `.cpp` | The system: lights, shadow map, uniform upload, serialization |
| `src/gameLayer/WorldEditorTools/LightingInspector.cpp` | The `Ctrl+8` authoring panel |
| `docs/LightingSystemPlan.md` | The development plan this release was built from |

No `CMakeLists.txt` change was needed: sources are globbed and `resources/` is already copied post-build.

---

## Authoring Workflow

1. `F1` opens the World Editor, `Ctrl+8` the Lighting panel.
2. Fly the editor camera to where the light belongs and press **Add Point Light At Camera**.
3. Tune colour, intensity and range. `Range` is where the light reaches exactly zero.
4. For a key light, add a directional light, aim it with `Direction`, and tick **Casts Shadow**.
5. Set the mood with ambient colour/strength and fog colour/density.
6. Save through the World Settings panel — lights travel with the world file.

### Default Rig

Installed on a fresh project and as the fallback for any pre-lighting save:

| Light | Type | Colour | Intensity | Notes |
|---|---|---|---|---|
| Moonlight | Directional | cold blue `150,172,210` | 0.55 | owns the shadow map |
| Spawn Lamp | Point | warm `255,196,130` | 1.10 | range 24, at `(0, 6, 0)` |

Ambient `40,44,58` @ 0.14 · Fog `8,9,12` @ 0.030 · Exposure 1.0 · Shadow distance 60 · Bias 0.0018

---

## Verification

- Builds clean under MSVC (VS 18 Insiders, x64 Debug), no new warnings.
- Runtime log confirms both shader stages compile, the program links, the depth framebuffer reports complete at
  4096×4096, the shader reaches all asset models, and shutdown frees the framebuffer and program exactly once.
- **Lighting confirmed by capture:** warm point-light falloff pooling on the floor, per-face directional shading on
  world geometry, and fog dissolving the distance.
- **Shadows confirmed by a controlled A/B capture** — identical camera pose, only `shadowsEnabled` differing. Surfaces
  occluded by an overhead slab lose their direct light with shadows on and regain it with shadows off.

---

## Known Limitations

- **One shadow-casting light, and it must be directional.** Point and spot lights are lit but cast nothing; a point
  light needs a cubemap and a spot light its own perspective map.
- **Eight-light hard limit.** The shader array is fixed at compile time. Extra lights in a world file are dropped with
  a warning rather than silently, but there is no light-volume culling yet — nearest-N selection is the next step.
- **Shadows are limited to `shadowDistance`.** Geometry outside the light camera's box renders lit rather than
  shadowed (the deliberate alternative — treating unknown depth as shadowed — paints everything distant black).
  Cascades would lift this.
- **No gamma/linear pipeline.** Textures load as plain `UNCOMPRESSED_R8G8B8A8` with no sRGB decode, so the shader
  deliberately does not gamma-encode its output; encoding without a matching decode washes every surface out. Going
  linear is an engine-wide change touching texture loading, the 2D/UI pass and every authored colour.
- **Albedo only.** Normal, roughness and occlusion maps are not sampled — the vendored AmbientCG sets only ship
  `_Color`.
- **Debug draws stay unlit.** `DrawGrid`, wireframes, bounding boxes and direction gizmos go through the rlgl batch
  shader by design.
- **The 2D/UI and mini-game passes are unaffected**, as intended.

---

## Next Steps

Each of these needs an art-direction decision before it earns its render budget:

- Nearest-N light selection and light-volume culling.
- Point/spot shadows (cubemap or per-light atlas).
- Cascaded shadow maps for larger play spaces.
- A linear pipeline with tonemapping, then bloom — the `PostProcessing` setting is still dormant.
- Animated lights (flicker, pulse, failing-fluorescent) — `LightingSystem::Update` already takes `deltaTime` for this.
- Normal/roughness sampling, once the texture sets are re-exported with their full map complement.
