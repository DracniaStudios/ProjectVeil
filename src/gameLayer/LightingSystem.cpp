#include "LightingSystem.h"

#include <rlgl.h>

#include <AssetManager.h>
#include <SceneManager.h>
#include <Settings.h>

#include <cmath>
#include <iostream>

namespace
{
	constexpr const char* SHADER_VS_PATH = RESOURCES_PATH "shaders/glsl330/lighting.vs";
	constexpr const char* SHADER_FS_PATH = RESOURCES_PATH "shaders/glsl330/lighting.fs";

	// raylib's PixelFormat has no depth entries; 19 is UNCOMPRESSED_R32 in the
	// public enum and is what raylib's own shadowmap sample stores for a
	// 24-bit depth attachment. Only the width/height/id fields are ever used.
	constexpr int DEPTH_TEXTURE_FORMAT = 19;

	// Normalize, falling back to `fallback` when the vector is degenerate.
	// Light::direction is an inspector field the user can zero out, and a NaN
	// direction poisons every fragment the light touches.
	Vector3 SafeNormalize(Vector3 vector, Vector3 fallback)
	{
		const float lengthSquared = Vector3LengthSqr(vector);
		if (lengthSquared <= 0.000001f) { return fallback; }
		return Vector3Scale(vector, 1.0f / sqrtf(lengthSquared));
	}

	/**
	 * Build a depth-only framebuffer whose depth attachment is a *texture*.
	 *
	 * LoadRenderTexture() cannot be used here: it attaches depth as a
	 * renderbuffer (rlLoadTextureDepth(w, h, true)), which the GPU can render
	 * into but a shader can never sample. Passing useRenderBuffer=false gives a
	 * real texture instead.
	 *
	 * The returned RenderTexture2D is assembled by hand so BeginTextureMode()
	 * still works — it only reads .id and .texture.width/height.
	 */
	RenderTexture2D LoadShadowMapTarget(int resolution)
	{
		RenderTexture2D target = {};

		target.id = rlLoadFramebuffer();
		if (target.id == 0)
		{
			std::cerr << "[Lighting] Failed to create the shadow map framebuffer\n";
			return target;
		}

		rlEnableFramebuffer(target.id);

		target.depth.id = rlLoadTextureDepth(resolution, resolution, false);
		target.depth.width = resolution;
		target.depth.height = resolution;
		target.depth.format = DEPTH_TEXTURE_FORMAT;
		target.depth.mipmaps = 1;

		// BeginTextureMode() reads these to size the viewport and to give
		// BeginMode3D() an aspect ratio (1.0 for a square map).
		target.texture.width = resolution;
		target.texture.height = resolution;

		rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

		// No colour attachment is created on purpose — the depth pass writes
		// nothing but depth. A depth-only FBO is complete under GL 3.3.
		const bool complete = rlFramebufferComplete(target.id);
		rlDisableFramebuffer();

		if (!complete)
		{
			std::cerr << "[Lighting] Shadow map framebuffer is incomplete — shadows disabled\n";
			rlUnloadFramebuffer(target.id);
			return RenderTexture2D{};
		}

		std::cout << "[Lighting] Shadow map ready: " << resolution << "x" << resolution << "\n";
		return target;
	}

	void UnloadShadowMapTarget(RenderTexture2D& target)
	{
		if (target.id == 0) { return; }

		// rlUnloadFramebuffer queries the depth attachment and deletes the
		// texture itself, so calling UnloadTexture(target.depth) as well would
		// be a double free.
		rlUnloadFramebuffer(target.id);
		target = RenderTexture2D{};
	}
}

#pragma region Light Save Data
Json Light::formatToJson() const
{
	Json j;
	j["Name"] = name;
	j["Type"] = static_cast<int>(type);
	j["IsEnabled"] = isEnabled;
	j["CastsShadow"] = castsShadow;

	j["Position"] = { position.x, position.y, position.z };
	j["Direction"] = { direction.x, direction.y, direction.z };

	// Same {r,g,b,a} shape GameObject::addCommonToJson uses
	j["Color"] = { color.r, color.g, color.b, color.a };
	j["Intensity"] = intensity;
	j["Range"] = range;
	j["InnerCone"] = innerConeDegrees;
	j["OuterCone"] = outerConeDegrees;
	return j;
}

bool Light::loadFromJson(const Json& j)
{
	if (!j.is_object()) { return false; }

	name = j.value("Name", name);

	const int loadedType = j.value("Type", static_cast<int>(type));
	type = (loadedType >= 0 && loadedType < LIGHT_TYPE_COUNT)
		? static_cast<LightType>(loadedType)
		: LIGHT_POINT;

	isEnabled = j.value("IsEnabled", true);
	castsShadow = j.value("CastsShadow", false);

	if (j.contains("Position") && j["Position"].is_array() && j["Position"].size() == 3)
	{
		position = Vector3{ j["Position"][0], j["Position"][1], j["Position"][2] };
	}

	if (j.contains("Direction") && j["Direction"].is_array() && j["Direction"].size() == 3)
	{
		direction = Vector3{ j["Direction"][0], j["Direction"][1], j["Direction"][2] };
	}

	if (j.contains("Color") && j["Color"].is_array() && j["Color"].size() == 4)
	{
		color.r = j["Color"][0];
		color.g = j["Color"][1];
		color.b = j["Color"][2];
		color.a = j["Color"][3];
	}

	intensity = j.value("Intensity", intensity);
	range = j.value("Range", range);
	innerConeDegrees = j.value("InnerCone", innerConeDegrees);
	outerConeDegrees = j.value("OuterCone", outerConeDegrees);
	return true;
}
#pragma endregion

#pragma region Lifecycle
bool LightingSystem::Init()
{
	if (isReady) { return true; }

	// Pull quality settings before allocating anything, so the shadow map is
	// created at the requested resolution the first time round.
	ApplySettings();

	lightShader = LoadShader(SHADER_VS_PATH, SHADER_FS_PATH);
	if (!IsShaderValid(lightShader))
	{
		std::cerr << "[Lighting] Failed to load lighting shader:\n  " << SHADER_VS_PATH
			<< "\n  " << SHADER_FS_PATH << "\n[Lighting] Falling back to raylib's unlit shader\n";

		// LoadShader allocates the locs array even on failure — release it
		UnloadShader(lightShader);
		lightShader = Shader{};
		isReady = false;
		return false;
	}

	// raylib resolves mvp/matModel/matNormal/colDiffuse/texture0 by name, but
	// NOT viewPos — SHADER_LOC_VECTOR_VIEW has to be wired up by hand.
	lightShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightShader, "viewPos");

	CacheUniformLocations();

	if (shadowsEnabled)
	{
		shadowMap = LoadShadowMapTarget(shadowMapResolution);
		if (shadowMap.id == 0) { shadowsEnabled = false; }
	}

	// The flashlight is a spot light that never enters `lights`
	flashlight.name = "Flashlight";
	flashlight.type = LIGHT_SPOT;
	flashlight.color = Color{ 255, 246, 224, 255 };
	flashlight.intensity = 1.6f;
	flashlight.range = 26.0f;
	flashlight.innerConeDegrees = 11.0f;
	flashlight.outerConeDegrees = 24.0f;
	flashlight.isEnabled = true;

	isReady = true;

	// Shared AssetManager models are bound into objects by pointer, so applying
	// the shader here covers every object that will ever reference them.
	ApplyToLoadedAssets();

	if (lights.empty()) { CreateDefaultRig(); }

	std::cout << "[Lighting] Lighting system ready (shader id " << lightShader.id
		<< ", " << lights.size() << " lights)\n";
	return true;
}

void LightingSystem::Shutdown()
{
	// Materials hold a *copy* of the Shader struct, not ownership, so every
	// model's material shader id dangles after this point. Shutdown must
	// therefore be the last engine call before CloseWindow().
	UnloadShadowMapTarget(shadowMap);

	if (lightShader.id != 0)
	{
		UnloadShader(lightShader);
		lightShader = Shader{};
	}

	uniforms = Uniforms{};
	isReady = false;
	std::cout << "[Lighting] Shutdown complete\n";
}

void LightingSystem::CacheUniformLocations()
{
	uniforms.lightCount = GetShaderLocation(lightShader, "lightCount");
	uniforms.lightType = GetShaderLocation(lightShader, "lightType");
	uniforms.lightPosition = GetShaderLocation(lightShader, "lightPosition");
	uniforms.lightDirection = GetShaderLocation(lightShader, "lightDirection");
	uniforms.lightColor = GetShaderLocation(lightShader, "lightColor");
	uniforms.lightRange = GetShaderLocation(lightShader, "lightRange");
	uniforms.lightCone = GetShaderLocation(lightShader, "lightCone");

	uniforms.ambient = GetShaderLocation(lightShader, "ambient");
	uniforms.fogColor = GetShaderLocation(lightShader, "fogColor");
	uniforms.fogDensity = GetShaderLocation(lightShader, "fogDensity");
	uniforms.exposure = GetShaderLocation(lightShader, "exposure");
	uniforms.specularStrength = GetShaderLocation(lightShader, "specularStrength");
	uniforms.shininess = GetShaderLocation(lightShader, "shininess");

	uniforms.shadowMap = GetShaderLocation(lightShader, "shadowMap");
	uniforms.lightVP = GetShaderLocation(lightShader, "lightVP");
	uniforms.shadowTexelSize = GetShaderLocation(lightShader, "shadowTexelSize");
	uniforms.shadowBias = GetShaderLocation(lightShader, "shadowBias");
	uniforms.shadowSoftness = GetShaderLocation(lightShader, "shadowSoftness");
	uniforms.shadowsEnabled = GetShaderLocation(lightShader, "shadowsEnabled");
	uniforms.shadowLightIndex = GetShaderLocation(lightShader, "shadowLightIndex");

	uniforms.depthPass = GetShaderLocation(lightShader, "depthPass");
}

bool LightingSystem::ResizeShadowMap(int resolution)
{
	shadowMapResolution = static_cast<int>(Clamp(static_cast<float>(resolution), 256.0f, 8192.0f));

	UnloadShadowMapTarget(shadowMap);

	// Before Init(), or while shadows are switched off, the resolution is just
	// remembered — Update() allocates lazily the moment it is needed.
	if (!isReady || !shadowsEnabled) { return true; }

	shadowMap = LoadShadowMapTarget(shadowMapResolution);
	if (shadowMap.id == 0)
	{
		shadowsEnabled = false;
		return false;
	}
	return true;
}
#pragma endregion

#pragma region Material Injection
void LightingSystem::ApplyToModel(Model& model) const
{
	// A no-op before Init(): every model is rebound through
	// GameObject::loadVisuals() anyway, and ApplyToLoadedAssets()/ApplyToScene()
	// catch anything created earlier.
	if (!isReady) { return; }
	if (model.materials == nullptr) { return; }

	for (int i = 0; i < model.materialCount; i++)
	{
		model.materials[i].shader = lightShader;
	}
}

void LightingSystem::ApplyToLoadedAssets() const
{
	if (!isReady) { return; }

	int applied = 0;
	for (auto& asset : AssetManager::getInstance().assets)
	{
		if (asset.type != ASSET_MODEL) { continue; }
		if (asset.model.meshCount == 0) { continue; }

		ApplyToModel(asset.model);
		applied++;
	}

	std::cout << "[Lighting] Applied lighting shader to " << applied << " asset models\n";
}

void LightingSystem::ApplyToScene(Scene* scene) const
{
	if (!isReady || scene == nullptr) { return; }

	for (auto& object : scene->gameMap.gameObjects) { ApplyToModel(object.model); }
	for (auto& [id, entity] : scene->entities) { ApplyToModel(entity->model); }
	for (auto& [id, interactable] : scene->interactables) { ApplyToModel(interactable->model); }

	if (scene->player != nullptr)
	{
		ApplyToModel(scene->player->model);
		if (scene->player->artifact != nullptr) { ApplyToModel(scene->player->artifact->model); }
	}
}
#pragma endregion

#pragma region Light Management
Light* LightingSystem::AddLight(const Light& light)
{
	// Pointer is valid only until the next AddLight/RemoveLight — the editor
	// re-resolves through an index each frame rather than caching this.
	lights.push_back(light);
	return &lights.back();
}

void LightingSystem::RemoveLight(int index)
{
	if (index < 0 || index >= static_cast<int>(lights.size())) { return; }
	lights.erase(lights.begin() + index);

	// The shadow owner is an index into `lights` and is now stale; the next
	// Update() re-runs SelectShadowLight and rebuilds it.
	shadowLightIndex = -1;
}

void LightingSystem::ClearLights()
{
	lights.clear();
	shadowLightIndex = -1;
}

void LightingSystem::CreateDefaultRig()
{
	lights.clear();
	shadowLightIndex = -1;

	// Cold overhead key light, angled so vertical faces read differently from
	// horizontal ones. This is the light that owns the shadow map.
	Light moon = {};
	moon.name = "Moonlight";
	moon.type = LIGHT_DIRECTIONAL;
	moon.direction = Vector3{ -0.45f, -1.0f, -0.35f };
	moon.color = Color{ 150, 172, 210, 255 };
	moon.intensity = 0.55f;
	moon.castsShadow = true;
	lights.push_back(moon);

	// Warm fill near the origin so the spawn area is not read as flat.
	Light lamp = {};
	lamp.name = "Spawn Lamp";
	lamp.type = LIGHT_POINT;
	lamp.position = Vector3{ 0.0f, 6.0f, 0.0f };
	lamp.color = Color{ 255, 196, 130, 255 };
	lamp.intensity = 1.1f;
	lamp.range = 24.0f;
	lights.push_back(lamp);

	std::cout << "[Lighting] Installed the default lighting rig\n";
}
#pragma endregion

#pragma region Per-Frame Upload
void LightingSystem::Update(const Camera3D& camera, float deltaTime)
{
	(void)deltaTime; // No animated lights yet; kept for flicker/pulse support

	if (!isReady) { return; }

	// Lazily allocate the depth target. Shadows can be switched on at runtime —
	// the inspector checkbox or a Settings change — after Init() skipped it,
	// and without this the toggle would appear to do nothing.
	if (shadowsEnabled && shadowMap.id == 0)
	{
		shadowMap = LoadShadowMapTarget(shadowMapResolution);
		if (shadowMap.id == 0) { shadowsEnabled = false; }
	}

	// Resolve the shadow owner before packing, so UploadLights() can map it to
	// the packed slot the shader will actually compare against.
	SelectShadowLight();

	// The flashlight rides the active camera (player or editor)
	if (flashlightEnabled)
	{
		flashlight.position = camera.position;
		flashlight.direction = SafeNormalize(
			Vector3Subtract(camera.target, camera.position),
			Vector3{ 0.0f, 0.0f, -1.0f });
	}

	// viewPos drives both the specular term and the fog distance
	SetShaderValue(lightShader, lightShader.locs[SHADER_LOC_VECTOR_VIEW],
		&camera.position, SHADER_UNIFORM_VEC3);

	UploadLights();
	UploadAtmosphere();
}

void LightingSystem::UploadLights()
{
	// Always sized to LIGHT_LIMIT and uploaded whole: a shrinking light list
	// would otherwise leave last frame's values in the unused slots.
	int types[LIGHT_LIMIT] = {};
	float positions[LIGHT_LIMIT * 3] = {};
	float directions[LIGHT_LIMIT * 3] = {};
	float colors[LIGHT_LIMIT * 4] = {};
	float ranges[LIGHT_LIMIT] = {};
	float cones[LIGHT_LIMIT * 2] = {};

	int count = 0;
	int packedShadowIndex = -1;

	auto pack = [&](const Light& light, int sourceIndex)
	{
		if (count >= LIGHT_LIMIT || !light.isEnabled) { return; }

		const int slot = count;

		types[slot] = static_cast<int>(light.type);

		positions[slot * 3 + 0] = light.position.x;
		positions[slot * 3 + 1] = light.position.y;
		positions[slot * 3 + 2] = light.position.z;

		const Vector3 direction = SafeNormalize(light.direction, Vector3{ 0.0f, -1.0f, 0.0f });
		directions[slot * 3 + 0] = direction.x;
		directions[slot * 3 + 1] = direction.y;
		directions[slot * 3 + 2] = direction.z;

		colors[slot * 4 + 0] = light.color.r / 255.0f;
		colors[slot * 4 + 1] = light.color.g / 255.0f;
		colors[slot * 4 + 2] = light.color.b / 255.0f;
		colors[slot * 4 + 3] = fmaxf(light.intensity, 0.0f);

		ranges[slot] = fmaxf(light.range, 0.0001f);

		// cos() shrinks as the angle grows, so cos(inner) must stay strictly
		// above cos(outer) or the shader's smoothstep divides by zero.
		const float inner = Clamp(light.innerConeDegrees, 0.0f, 89.0f);
		const float outer = Clamp(light.outerConeDegrees, 0.0f, 89.9f);
		float cosOuter = cosf(outer * DEG2RAD);
		float cosInner = cosf(inner * DEG2RAD);
		if (cosInner <= cosOuter) { cosInner = cosOuter + 0.001f; }

		cones[slot * 2 + 0] = cosInner;
		cones[slot * 2 + 1] = cosOuter;

		// Only a directional light can drive the single shadow map
		if (sourceIndex >= 0 && sourceIndex == shadowLightIndex)
		{
			packedShadowIndex = slot;
		}

		count++;
	};

	// The flashlight is packed FIRST so eight authored world lights can never
	// evict the player's own light source — that would be a gameplay bug.
	if (flashlightEnabled) { pack(flashlight, -1); }

	for (int i = 0; i < static_cast<int>(lights.size()); i++) { pack(lights[i], i); }

	activeLightCount = count;

	SetShaderValue(lightShader, uniforms.lightCount, &count, SHADER_UNIFORM_INT);
	SetShaderValueV(lightShader, uniforms.lightType, types, SHADER_UNIFORM_INT, LIGHT_LIMIT);
	SetShaderValueV(lightShader, uniforms.lightPosition, positions, SHADER_UNIFORM_VEC3, LIGHT_LIMIT);
	SetShaderValueV(lightShader, uniforms.lightDirection, directions, SHADER_UNIFORM_VEC3, LIGHT_LIMIT);
	SetShaderValueV(lightShader, uniforms.lightColor, colors, SHADER_UNIFORM_VEC4, LIGHT_LIMIT);
	SetShaderValueV(lightShader, uniforms.lightRange, ranges, SHADER_UNIFORM_FLOAT, LIGHT_LIMIT);
	SetShaderValueV(lightShader, uniforms.lightCone, cones, SHADER_UNIFORM_VEC2, LIGHT_LIMIT);

	// The shader compares against the *packed* slot, not the lights[] index —
	// disabled lights and the flashlight shift everything after them.
	SetShaderValue(lightShader, uniforms.shadowLightIndex, &packedShadowIndex, SHADER_UNIFORM_INT);
}

void LightingSystem::UploadAtmosphere()
{
	const float ambientValues[4] = {
		ambientColor.r / 255.0f,
		ambientColor.g / 255.0f,
		ambientColor.b / 255.0f,
		fmaxf(ambientStrength, 0.0f)
	};
	SetShaderValue(lightShader, uniforms.ambient, ambientValues, SHADER_UNIFORM_VEC4);

	const float fogValues[4] = {
		fogColor.r / 255.0f,
		fogColor.g / 255.0f,
		fogColor.b / 255.0f,
		fogColor.a / 255.0f
	};
	SetShaderValue(lightShader, uniforms.fogColor, fogValues, SHADER_UNIFORM_VEC4);

	const float density = fmaxf(fogDensity, 0.0f);
	SetShaderValue(lightShader, uniforms.fogDensity, &density, SHADER_UNIFORM_FLOAT);

	const float clampedExposure = fmaxf(exposure, 0.0f);
	SetShaderValue(lightShader, uniforms.exposure, &clampedExposure, SHADER_UNIFORM_FLOAT);

	const float specular = fmaxf(specularStrength, 0.0f);
	SetShaderValue(lightShader, uniforms.specularStrength, &specular, SHADER_UNIFORM_FLOAT);

	const float gloss = fmaxf(shininess, 1.0f);
	SetShaderValue(lightShader, uniforms.shininess, &gloss, SHADER_UNIFORM_FLOAT);

	const float texelSize = 1.0f / static_cast<float>(shadowMapResolution > 0 ? shadowMapResolution : 1);
	SetShaderValue(lightShader, uniforms.shadowTexelSize, &texelSize, SHADER_UNIFORM_FLOAT);

	const float bias = fmaxf(shadowBias, 0.0f);
	SetShaderValue(lightShader, uniforms.shadowBias, &bias, SHADER_UNIFORM_FLOAT);

	const float softness = fmaxf(shadowSoftness, 0.0f);
	SetShaderValue(lightShader, uniforms.shadowSoftness, &softness, SHADER_UNIFORM_FLOAT);

	// Single source of truth for the shader's shadow gate: shadows are only
	// sampled when they are enabled, a framebuffer exists, and a directional
	// light actually claimed it this frame. Anything else and the shader would
	// read a stale or never-written depth texture.
	const int shadowGate = (shadowsEnabled && shadowMap.id != 0 && shadowLightIndex >= 0) ? 1 : 0;
	SetShaderValue(lightShader, uniforms.shadowsEnabled, &shadowGate, SHADER_UNIFORM_INT);
}
#pragma endregion

#pragma region Shadow Pass
void LightingSystem::DrawShadowCasters(Scene* scene) const
{
	// Geometry only. Scene_drawScene3D() also runs scene->draw3D() (DrawGrid),
	// DrawModelWires, DrawBoundingBox and the direction-debug spheres — all of
	// which render through the rlgl batch shader and would stamp grid lines and
	// debug gizmos into the shadow map as real world shadows.
	auto drawCaster = [](GameObject& object)
	{
		if (!object.isEnabled || !object.display3DModel || !object.castsShadow) { return; }
		if (object.model.meshCount == 0) { return; }

		// Rebuilt here rather than reused: render3D() has not run yet this
		// frame, so model.transform still holds last frame's pose.
		const Vector3 scale = object.rigidBody3D.scale;
		object.model.transform = MatrixMultiply(
			MatrixScale(scale.x, scale.y, scale.z),
			QuaternionToMatrix(object.rigidBody3D.rotation));

		DrawModel(object.model, object.rigidBody3D.translation, 1.0f, object.defaultColor);
	};

	for (auto& object : scene->gameMap.gameObjects) { drawCaster(object); }
	for (auto& [id, entity] : scene->entities) { drawCaster(*entity); }
	for (auto& [id, interactable] : scene->interactables) { drawCaster(*interactable); }

	if (scene->player != nullptr)
	{
		drawCaster(*scene->player);

		// The artifact hovers a unit in front of the camera; Scene_new() clears
		// its castsShadow flag so it cannot smear a shadow across the view.
		if (scene->player->artifact != nullptr) { drawCaster(*scene->player->artifact); }
	}
}

void LightingSystem::SelectShadowLight()
{
	shadowLightIndex = -1;

	// Only a directional light qualifies: the map is a single orthographic
	// depth render, which cannot represent a point light's six directions or a
	// spot light's perspective frustum.
	if (!shadowsEnabled || shadowMap.id == 0) { return; }

	for (int i = 0; i < static_cast<int>(lights.size()); i++)
	{
		const Light& light = lights[i];
		if (light.isEnabled && light.castsShadow && light.type == LIGHT_DIRECTIONAL)
		{
			shadowLightIndex = i;
			return;
		}
	}
}

void LightingSystem::RenderShadowPass(Scene* scene)
{
	if (!isReady || scene == nullptr) { return; }

	// Owner (and the shader's shadow gate) were resolved in Update()
	if (shadowLightIndex < 0 || shadowLightIndex >= static_cast<int>(lights.size())) { return; }

	const Light& light = lights[shadowLightIndex];
	const Vector3 lightDirection = SafeNormalize(light.direction, Vector3{ 0.0f, -1.0f, 0.0f });

	// Centre the finite map on the player so it follows the play area
	const Vector3 focus = (scene->player != nullptr) ? scene->player->getPosition() : Vector3Zero();

	Camera3D lightCamera = {};
	lightCamera.projection = CAMERA_ORTHOGRAPHIC;
	lightCamera.fovy = shadowDistance; // Orthographic: fovy is the vertical extent
	lightCamera.target = focus;
	lightCamera.position = Vector3Subtract(focus, Vector3Scale(lightDirection, shadowDistance));

	// MatrixLookAt degenerates when `up` is parallel to the view direction, so
	// a near-vertical light needs a different reference axis.
	lightCamera.up = (fabsf(lightDirection.y) > 0.99f)
		? Vector3{ 0.0f, 0.0f, 1.0f }
		: Vector3{ 0.0f, 1.0f, 0.0f };

	// depthPass makes the fragment shader return immediately. Without it the
	// shader would sample shadowMap while that same texture is attached as the
	// depth target — undefined behaviour in OpenGL.
	const int depthPassOn = 1;
	const int depthPassOff = 0;
	SetShaderValue(lightShader, uniforms.depthPass, &depthPassOn, SHADER_UNIFORM_INT);

	BeginTextureMode(shadowMap);
		ClearBackground(WHITE);
		BeginMode3D(lightCamera);
			// Captured inside BeginMode3D so rlgl holds the light's matrices.
			// MatrixMultiply(view, projection) matches raylib's row-vector
			// convention, so the shader can do lightVP*vec4(worldPos, 1.0).
			lightViewProj = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
			DrawShadowCasters(scene);
		EndMode3D();
	EndTextureMode();

	SetShaderValue(lightShader, uniforms.depthPass, &depthPassOff, SHADER_UNIFORM_INT);
	SetShaderValueMatrix(lightShader, uniforms.lightVP, lightViewProj);
}

void LightingSystem::BindShadowMap()
{
	if (!isReady || uniforms.shadowMap < 0) { return; }
	if (shadowLightIndex < 0 || shadowMap.depth.id == 0) { return; }

	// Bound to SHADOW_MAP_SLOT (15) because raylib's DrawMesh() owns slots
	// 0..MAX_MATERIAL_MAPS-1 (12) and rebinds them on every draw call.
	rlActiveTextureSlot(SHADOW_MAP_SLOT);
	rlEnableTexture(shadowMap.depth.id);

	// The sampler only needs the unit index, and it persists in the program
	// object across raylib's own rlEnableShader/rlDisableShader calls.
	const int slot = SHADOW_MAP_SLOT;
	SetShaderValue(lightShader, uniforms.shadowMap, &slot, SHADER_UNIFORM_INT);
}
#pragma endregion

#pragma region Settings & Save Data
void LightingSystem::ApplySettings()
{
	auto& settings = Settings::getInstance();

	// Video > Shadows is authored 0-4 like every other quality selection.
	// 0 turns shadows off; 1-4 pick a shadow map resolution.
	const int quality = static_cast<int>(Clamp(settings.shadows.value, 0.0f, 4.0f));
	const int resolutions[5] = { 1024, 512, 1024, 2048, 4096 };

	shadowsEnabled = quality > 0;
	shadowSoftness = (quality >= 3) ? 1.0f : 0.5f;

	// Luminosity is a 0-2 multiplier, but a freshly written settings.json
	// stores 0 for anything never touched — which would render a black frame.
	exposure = (settings.luminosity.value > 0.0f) ? settings.luminosity.value : 1.0f;

	// Only reallocate on an actual size change. Switching shadows back on with
	// the size unchanged is handled by Update()'s lazy allocation.
	if (resolutions[quality] != shadowMapResolution) { ResizeShadowMap(resolutions[quality]); }
}

Json LightingSystem::formatToJson() const
{
	Json j;

	j["Ambient"] = { ambientColor.r, ambientColor.g, ambientColor.b, ambientColor.a };
	j["AmbientStrength"] = ambientStrength;

	j["FogColor"] = { fogColor.r, fogColor.g, fogColor.b, fogColor.a };
	j["FogDensity"] = fogDensity;

	j["Exposure"] = exposure;
	j["SpecularStrength"] = specularStrength;
	j["Shininess"] = shininess;

	j["ShadowsEnabled"] = shadowsEnabled;
	j["ShadowBias"] = shadowBias;
	j["ShadowDistance"] = shadowDistance;
	j["ShadowSoftness"] = shadowSoftness;
	j["ShadowResolution"] = shadowMapResolution;

	// The flashlight is player state, not level data, so it is not saved here.
	Json lightArray = Json::array();
	for (const auto& light : lights) { lightArray.push_back(light.formatToJson()); }
	j["Lights"] = lightArray;

	return j;
}

bool LightingSystem::loadFromJson(const Json& j)
{
	if (!j.is_object()) { return false; }

	if (j.contains("Ambient") && j["Ambient"].is_array() && j["Ambient"].size() == 4)
	{
		ambientColor.r = j["Ambient"][0];
		ambientColor.g = j["Ambient"][1];
		ambientColor.b = j["Ambient"][2];
		ambientColor.a = j["Ambient"][3];
	}
	ambientStrength = j.value("AmbientStrength", ambientStrength);

	if (j.contains("FogColor") && j["FogColor"].is_array() && j["FogColor"].size() == 4)
	{
		fogColor.r = j["FogColor"][0];
		fogColor.g = j["FogColor"][1];
		fogColor.b = j["FogColor"][2];
		fogColor.a = j["FogColor"][3];
	}
	fogDensity = j.value("FogDensity", fogDensity);

	exposure = j.value("Exposure", exposure);
	specularStrength = j.value("SpecularStrength", specularStrength);
	shininess = j.value("Shininess", shininess);

	shadowBias = j.value("ShadowBias", shadowBias);
	shadowDistance = j.value("ShadowDistance", shadowDistance);
	shadowSoftness = j.value("ShadowSoftness", shadowSoftness);

	const int savedResolution = j.value("ShadowResolution", shadowMapResolution);
	if (savedResolution != shadowMapResolution) { ResizeShadowMap(savedResolution); }

	// Only honour "shadows on" if the framebuffer actually exists
	const bool savedShadows = j.value("ShadowsEnabled", shadowsEnabled);
	shadowsEnabled = savedShadows && (!isReady || shadowMap.id != 0);

	lights.clear();
	shadowLightIndex = -1;

	if (j.contains("Lights") && j["Lights"].is_array())
	{
		for (const auto& lightData : j["Lights"])
		{
			if (static_cast<int>(lights.size()) >= LIGHT_LIMIT)
			{
				std::cerr << "[Lighting] World defines more than " << LIGHT_LIMIT
					<< " lights — the extras were dropped\n";
				break;
			}

			Light light = {};
			if (!light.loadFromJson(lightData)) { continue; }
			lights.push_back(light);
		}
	}

	// A world with no lights renders black and reads as a broken build, so an
	// empty (or absent) light list falls back to the default rig.
	if (lights.empty()) { CreateDefaultRig(); }

	std::cout << "[Lighting] Loaded " << lights.size() << " lights from save data\n";
	return true;
}
#pragma endregion
