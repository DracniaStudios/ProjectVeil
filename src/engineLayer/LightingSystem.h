#pragma once
#ifndef LIGHTING_SYSTEM_H
#define LIGHTING_SYSTEM_H

#include <raylib.h>
#include <Physics.h> // Json (nlohmann) + the raymath operator overloads

#include <string>
#include <vector>

// Scene is only forward-declared: Scene.h pulls in GameObject.h, and
// GameObject.cpp includes this header to inject the shader on model rebind.
// Including Scene.h here would close that cycle.
struct Scene;

/**
 * Maximum lights uploaded to the shader in one pass.
 * MUST stay equal to MAX_LIGHTS in resources/shaders/glsl330/lighting.fs.
 */
inline constexpr int LIGHT_LIMIT = 8;

/**
 * Texture unit the shadow map is bound to.
 * raylib's DrawMesh() binds material maps to slots 0..MAX_MATERIAL_MAPS-1 (12)
 * and unbinds them again afterwards, so any slot below 12 can be clobbered
 * mid-frame. 15 is the top of OpenGL 3.3's guaranteed 16 texture units.
 */
inline constexpr int SHADOW_MAP_SLOT = 15;

// Must match the LIGHT_* defines in lighting.fs
enum LightType : int
{
	LIGHT_DIRECTIONAL = 0, // Sun/moon: direction only, no falloff
	LIGHT_POINT = 1,       // Omnidirectional, falls off to zero at `range`
	LIGHT_SPOT = 2,        // Cone, falls off to zero at `range` and outside `outerConeDegrees`
	LIGHT_TYPE_COUNT
};

inline const char* lightTypeToString(int type)
{
	switch (type)
	{
		case LIGHT_DIRECTIONAL: return "Directional";
		case LIGHT_POINT:       return "Point";
		case LIGHT_SPOT:        return "Spot";
		default:                return "Unknown";
	}
}

/** A single authored light source. */
struct Light
{
	std::string name = "Light";
	LightType type = LIGHT_POINT;

	bool isEnabled = true;

	// Only a directional light can own the shadow map (one map, one caster
	// view). Set on a point/spot light it is stored but ignored.
	bool castsShadow = false;

	Vector3 position = Vector3{ 0.0f, 5.0f, 0.0f };  // Unused by directional lights
	Vector3 direction = Vector3{ 0.0f, -1.0f, 0.0f }; // The way light travels; unused by point lights

	// Colour is a raylib Color to match every other authored colour in the
	// engine (GameObject::defaultColor, the editor colour pickers, the
	// {r,g,b,a} JSON arrays). Brightness beyond 0-255 lives in `intensity`.
	Color color = WHITE;
	float intensity = 1.0f;
	float range = 20.0f; // World units; attenuation is exactly 0 at this distance

	// Authored in degrees and converted to cosines only at upload time — the
	// inspector would be unusable with raw cosines.
	float innerConeDegrees = 20.0f;
	float outerConeDegrees = 35.0f;

	Json formatToJson() const;
	bool loadFromJson(const Json& j);
};

/**
 * Forward lighting, shadow mapping and atmosphere for the 3D scene.
 *
 * The shader is injected into every Material rather than pushed with
 * BeginShaderMode(): raylib's DrawMesh() calls rlEnableShader(material.shader.id)
 * itself, so BeginShaderMode() only affects the rlgl batch (DrawGrid, shapes,
 * 2D) and would leave every model unlit.
 */
class LightingSystem
{
	LightingSystem() = default;
	~LightingSystem() = default;

public:
	LightingSystem(const LightingSystem&) = delete;
	LightingSystem& operator=(const LightingSystem&) = delete;
	LightingSystem(LightingSystem&&) = delete;
	LightingSystem& operator=(LightingSystem&&) = delete;

	static LightingSystem& getInstance()
	{
		static LightingSystem instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	/** Lifecycle **/
	// Requires a live GL context (InitWindow) and must run BEFORE any scene is
	// constructed, so objects built during scene construction get the shader.
	bool Init();
	void Shutdown();
	bool IsReady() const { return isReady; }

	/** Per-Frame **/
	void Update(const Camera3D& camera, float deltaTime);
	void RenderShadowPass(Scene* scene);
	void BindShadowMap();

	/** Editor Gizmos **/
	// Debug geometry marking each light's position and the direction it throws
	// light. Drawn through the rlgl batch (raylib's default unlit shader), so
	// gizmos stay readable no matter how dark the scene is. SceneManager only
	// calls this while the World Editor is open — it is an authoring aid.
	void DrawGizmos() const;
	// The inspector's selected light gets a halo so it can be picked out of a rig
	void SetGizmoHighlight(int lightIndex) { gizmoHighlightIndex = lightIndex; }

	bool showGizmos = true;
	// Position markers and direction arrows always draw on top of geometry.
	// This extends that to the influence volumes (ranges and cones), which are
	// otherwise depth-tested so they read as sitting in the world.
	bool gizmoXray = false;

	/** Material Injection **/
	void ApplyToModel(Model& model) const;
	void ApplyToLoadedAssets() const;
	void ApplyToScene(Scene* scene) const;

	/** Light Management **/
	std::vector<Light>& GetLights() { return lights; }
	const std::vector<Light>& GetLights() const { return lights; }
	Light* AddLight(const Light& light);
	void RemoveLight(int index);
	void ClearLights();
	// Fallback rig used for worlds saved before lighting existed
	void CreateDefaultRig();

	/** Player Flashlight **/
	// Lives outside `lights` so it can never be deleted in the inspector or
	// written into a world file.
	void SetFlashlightEnabled(bool enabled) { flashlightEnabled = enabled; }
	void ToggleFlashlight() { flashlightEnabled = !flashlightEnabled; }
	bool IsFlashlightEnabled() const { return flashlightEnabled; }
	Light& GetFlashlight() { return flashlight; }

	/** Atmosphere (public: driven directly by the Lighting Inspector) **/
	Color ambientColor = Color{ 40, 44, 58, 255 };
	float ambientStrength = 0.14f;
	Color fogColor = Color{ 8, 9, 12, 255 };
	float fogDensity = 0.030f;
	float exposure = 1.0f;
	float specularStrength = 0.25f;
	float shininess = 16.0f;

	/** Shadow Tuning **/
	bool shadowsEnabled = true;
	float shadowBias = 0.0018f;
	float shadowDistance = 60.0f; // Orthographic extent of the shadow light's camera
	float shadowSoftness = 1.0f;  // PCF tap spacing, in shadow-map texels

	/** Diagnostics **/
	int GetActiveLightCount() const { return activeLightCount; }
	int GetShadowLightIndex() const { return shadowLightIndex; }
	int GetShadowMapResolution() const { return shadowMapResolution; }
	const Shader& GetShader() const { return lightShader; }

	// Reallocates the depth framebuffer; safe to call at runtime
	bool ResizeShadowMap(int resolution);

	/** Settings / Save Data **/
	void ApplySettings();
	Json formatToJson() const;
	bool loadFromJson(const Json& j);

private:
	// Cached uniform locations. Custom uniforms cannot live in Shader::locs —
	// that array is exactly RL_MAX_SHADER_LOCATIONS (32) entries and is owned
	// by raylib's own semantics.
	struct Uniforms
	{
		int lightCount = -1;
		int lightType = -1;
		int lightPosition = -1;
		int lightDirection = -1;
		int lightColor = -1;
		int lightRange = -1;
		int lightCone = -1;

		int ambient = -1;
		int fogColor = -1;
		int fogDensity = -1;
		int exposure = -1;
		int specularStrength = -1;
		int shininess = -1;

		int shadowMap = -1;
		int lightVP = -1;
		int shadowTexelSize = -1;
		int shadowBias = -1;
		int shadowSoftness = -1;
		int shadowsEnabled = -1;
		int shadowLightIndex = -1;

		int depthPass = -1;
	};

	void CacheUniformLocations();
	// Chooses which lights[] entry owns the shadow map. Runs at the top of
	// Update() so UploadLights() can translate it into a packed slot in the
	// same frame — selecting it inside RenderShadowPass() instead would leave
	// the shader comparing against last frame's index.
	void SelectShadowLight();
	void UploadLights();
	void UploadAtmosphere();
	void DrawShadowCasters(Scene* scene) const;

	// Gizmos are drawn in two layers so a light buried inside geometry can still
	// be found and aimed: InfluenceShapes is depth-tested, Markers is not.
	enum class GizmoLayer { InfluenceShapes, Markers };
	void DrawLightGizmo(const Light& light, bool highlighted, GizmoLayer layer) const;

	Shader lightShader = {};
	Uniforms uniforms = {};

	std::vector<Light> lights = {};

	Light flashlight = {};
	bool flashlightEnabled = false;

	// Hand-built depth-only framebuffer. LoadRenderTexture() cannot be used:
	// its depth buffer is a renderbuffer and is not sampleable.
	RenderTexture2D shadowMap = {};
	int shadowMapResolution = 2048;
	Matrix lightViewProj = {};
	int shadowLightIndex = -1; // Index into `lights`, -1 when nothing casts

	int activeLightCount = 0;
	int gizmoHighlightIndex = -1; // Index into `lights`, -1 for no selection
	bool isReady = false;
};

#endif
