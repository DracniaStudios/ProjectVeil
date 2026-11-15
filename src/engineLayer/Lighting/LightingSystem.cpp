#include "LightingSystem.h"

#include <Geometry.h>

#include <Render/Render.h>
#include <ResourcePaths.h>
#include <SceneManager.h>
#include <Settings.h>
#include <Client/LocalPlayer.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace Lighting
{
	static_assert(Render::kMaxLights == LIGHT_LIMIT, "Render::kMaxLights and Lighting::LIGHT_LIMIT must match MAX_LIGHTS in lighting.fs");

	// One of the lighting shader's files, as the C string raylib wants
	static std::string ShaderPath(const char* fileName)
	{
		return (ResourcePaths::Resources() / "shaders" / "glsl330" / fileName).string();
	}

	// Normalize, falling back to `fallback` when the vector is degenerate.
	// Light::direction is an inspector field the user can zero out, and a NaN
	// direction poisons every fragment the light touches.
	Vector3 SafeNormalize(Vector3 vector, Vector3 fallback)
	{
		const float lengthSquared = Vector3LengthSqr(vector);
		if (lengthSquared <= 0.000001f) { return fallback; }
		return Vector3Scale(vector, 1.0f / sqrtf(lengthSquared));
	}

	/** Editor Gizmo Drawing **/

	constexpr float GIZMO_BODY_RADIUS = 0.28f;
	constexpr float GIZMO_SUN_RAY_LENGTH = 4.0f;
	constexpr float GIZMO_SUN_SPREAD = 1.0f;

	// Build an orthonormal basis perpendicular to `normal`, for circles and
	// cones that have to face an arbitrary direction.
	void BuildBasis(Vector3 normal, Vector3& right, Vector3& up)
	{
		// Any reference axis works except one parallel to the normal, which
		// would make the cross product degenerate.
		const Vector3 reference = (fabsf(normal.y) > 0.99f)
			? Vector3{ 1.0f, 0.0f, 0.0f }
		: Vector3{ 0.0f, 1.0f, 0.0f };

		right = Vector3Normalize(Vector3CrossProduct(normal, reference));
		up = Vector3CrossProduct(normal, right); // Already unit: both inputs are unit and perpendicular
	}

	// Circle drawn from line segments. raylib's DrawCircle3D takes an axis and
	// an angle, which is awkward to derive from a plain direction vector; a
	// segment loop over an explicit basis keeps the orientation obvious.
	void DrawCircleFacing(Vector3 center, Vector3 normal, float radius, Color color, int segments = 24)
	{
		if (radius <= 0.0f || segments < 3) { return; }

		Vector3 right = {};
		Vector3 up = {};
		BuildBasis(normal, right, up);

		Vector3 previous = Vector3Add(center, Vector3Scale(right, radius));
		for (int i = 1; i <= segments; i++)
		{
			const float angle = (2.0f * PI * static_cast<float>(i)) / static_cast<float>(segments);
			const Vector3 point = Vector3Add(center,
				Vector3Add(Vector3Scale(right, cosf(angle) * radius),
					Vector3Scale(up, sinf(angle) * radius)));

			Render::DebugLine(previous, point, color);
			previous = point;
		}
	}

	// A shaft plus a cone head — the actual "which way does this light point"
	// indicator. DrawCylinderEx with a zero end radius gives the arrowhead.
	void DrawArrow(Vector3 origin, Vector3 direction, float length, float headRadius, Color color)
	{
		if (length <= 0.0f) { return; }

		const Vector3 tip = Vector3Add(origin, Vector3Scale(direction, length));
		const float headLength = fminf(headRadius * 3.0f, length * 0.5f);
		const Vector3 headBase = Vector3Subtract(tip, Vector3Scale(direction, headLength));

		Render::DebugLine(origin, headBase, color);
		Render::DebugCylinder(headBase, tip, headRadius, 0.0f, 8, color);
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

		// Pull the player's quality before allocating anything, so the shadow map
		// is created at the size it will be drawn at the first time round.
		ApplySettings();

		// Subscribed once, even if Init is retried after the shader fails to load
		if (!heardSettings)
		{
			Settings::Settings::getInstance().Subscribe({ "Shadows", "Luminosity" }, [this] { ApplySettings(); });
			heardSettings = true;
		}

		// The renderer loads the lighting shader in Render::Init
		if (Render::LightingShaderId() == 0)
		{
			std::cerr << "[Lighting] Failed to load lighting shader:\n  " << ShaderPath("lighting.vs")
				<< "\n  " << ShaderPath("lighting.fs") << "\n[Lighting] Falling back to raylib's unlit shader\n";
			isReady = false;
			return false;
		}

		RefreshShadowMap();

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

		if (lights.empty()) { CreateDefaultRig(); }

		std::cout << "[Lighting] Lighting system ready (shader id " << Render::LightingShaderId()
			<< ", " << lights.size() << " lights)\n";
		return true;
	}

	void LightingSystem::Shutdown()
	{
		// The shader belongs to the renderer and goes with Render::Shutdown; only the
		// shadow map is released here
		Render::SetShadowMapSize(0);

		isReady = false;
		std::cout << "[Lighting] Shutdown complete\n";
	}

	int LightingSystem::GetShadowMapResolution() const { return Render::ShadowMapSize(); }

	void LightingSystem::SetShadowResolution(int resolution)
	{
		shadowResolution = std::clamp(resolution, 256, 8192);
	}

	int LightingSystem::DrawnShadowMapSize() const
	{
		return ShadowsDrawn() ? std::min(shadowResolution, playerShadowLimit) : 0;
	}

	void LightingSystem::RefreshShadowMap()
	{
		const int wanted = DrawnShadowMapSize();
		if (wanted == GetShadowMapResolution()) { return; }

		if (wanted == 0)
		{
			Render::SetShadowMapSize(0);
			failedShadowMapSize = 0;
			return;
		}
		if (wanted == failedShadowMapSize) { return; }

		Render::SetShadowMapSize(0);
		failedShadowMapSize = Render::SetShadowMapSize(wanted) ? 0 : wanted;
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

#pragma region Per-Frame Packing
	void LightingSystem::Update(const Camera3D& camera, float deltaTime)
	{
		(void)deltaTime; // No animated lights yet; kept for flicker/pulse support

		if (!isReady) { return; }

		// Bring the depth target in line with what is drawn. The inspector, a
		// world load or a Settings change may have changed it since last frame,
		// and none of them can reallocate mid-pass themselves.
		RefreshShadowMap();

		// Resolve the shadow owner before packing, so SubmitLighting() can map it to
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

		SubmitLighting(camera);
	}

	void LightingSystem::SubmitLighting(const Camera3D& camera)
	{
		Render::LightingFrame frame;

		// viewPos drives both the specular term and the fog distance
		frame.viewPosition = camera.position;

		// The frame's light arrays are always kMaxLights long and are uploaded whole:
		// a shrinking light list would otherwise leave last frame's values in the
		// unused slots, which stay zero here.
		int count = 0;
		int packedShadowIndex = -1;

		auto pack = [&](const Light& light, int sourceIndex)
			{
				if (count >= LIGHT_LIMIT || !light.isEnabled) { return; }

				const int slot = count;
				Render::PackedLight& packed = frame.lights[slot];

				packed.type = static_cast<int>(light.type);

				packed.position = light.position;

				packed.direction = SafeNormalize(light.direction, Vector3{ 0.0f, -1.0f, 0.0f });

				packed.color = Vector4{
					light.color.r / 255.0f,
					light.color.g / 255.0f,
					light.color.b / 255.0f,
					fmaxf(light.intensity, 0.0f)
				};

				packed.range = fmaxf(light.range, 0.0001f);

				// cos() shrinks as the angle grows, so cos(inner) must stay strictly
				// above cos(outer) or the shader's smoothstep divides by zero.
				const float inner = Clamp(light.innerConeDegrees, 0.0f, 89.0f);
				const float outer = Clamp(light.outerConeDegrees, 0.0f, 89.9f);
				float cosOuter = cosf(outer * DEG2RAD);
				float cosInner = cosf(inner * DEG2RAD);
				if (cosInner <= cosOuter) { cosInner = cosOuter + 0.001f; }

				packed.cone = Vector2{ cosInner, cosOuter };

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
		frame.lightCount = count;

		// The shader compares against the *packed* slot, not the lights[] index —
		// disabled lights and the flashlight shift everything after them.
		frame.shadowLightSlot = packedShadowIndex;

		frame.ambient = Vector4{
			ambientColor.r / 255.0f,
			ambientColor.g / 255.0f,
			ambientColor.b / 255.0f,
			fmaxf(ambientStrength, 0.0f)
		};

		frame.fogColor = Vector4{
			fogColor.r / 255.0f,
			fogColor.g / 255.0f,
			fogColor.b / 255.0f,
			fogColor.a / 255.0f
		};

		frame.fogDensity = fmaxf(fogDensity, 0.0f);
		frame.exposure = fmaxf(DrawnExposure(), 0.0f);
		frame.specularStrength = fmaxf(specularStrength, 0.0f);
		frame.shininess = fmaxf(shininess, 1.0f);

		const int mapSize = GetShadowMapResolution();
		frame.shadowTexelSize = 1.0f / static_cast<float>(mapSize > 0 ? mapSize : 1);

		frame.shadowBias = fmaxf(shadowBias, 0.0f);
		frame.shadowSoftness = fmaxf(shadowSoftness, 0.0f);

		// Single source of truth for the shader's shadow gate: shadows are only
		// sampled when they are enabled, a framebuffer exists, and a directional
		// light actually claimed it this frame. Anything else and the shader would
		// read a stale or never-written depth texture.
		frame.shadowsEnabled = ShadowsDrawn() && Render::ShadowMapSize() > 0 && shadowLightIndex >= 0;

		Render::SetLighting(frame);
	}
#pragma endregion

#pragma region Shadow Pass
	void LightingSystem::DrawShadowCasters(SceneManagement::Scene* scene) const
	{
		// Geometry only. Scene_drawScene3D() also runs scene->draw3D() (DrawGrid),
		// DrawModelWires, DrawBoundingBox and the direction-debug spheres — all of
		// which render through the rlgl batch shader and would stamp grid lines and
		// debug gizmos into the shadow map as real world shadows.
		auto drawCaster = [](GameObject& object)
			{
				if (!object.isEnabled || !object.display3DModel || !object.castsShadow) { return; }
				if (!object.model) { return; }

				// Render::DrawModel builds the transform from the object's pose as it is now
				Render::DrawModel(object.MakeModelDraw());
			};

		scene->gameMap.ForEachObject(drawCaster);

		scene->ForEachPlayer([&](int, Player& player) {
			drawCaster(player);

			// The artifact hovers a unit in front of the camera; Scene_new() clears
			// its castsShadow flag so it cannot smear a shadow across the view.
			if (player.artifact) { drawCaster(*player.artifact); }
		});
	}

	void LightingSystem::SelectShadowLight()
	{
		shadowLightIndex = -1;

		// Only a directional light qualifies: the map is a single orthographic
		// depth render, which cannot represent a point light's six directions or a
		// spot light's perspective frustum.
		if (!ShadowsDrawn() || Render::ShadowMapSize() == 0) { return; }

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

	void LightingSystem::RenderShadowPass(SceneManagement::Scene* scene)
	{
		if (!isReady || scene == nullptr) { return; }

		// Owner (and the shader's shadow gate) were resolved in Update()
		if (shadowLightIndex < 0 || shadowLightIndex >= static_cast<int>(lights.size())) { return; }

		const Light& light = lights[shadowLightIndex];
		const Vector3 lightDirection = SafeNormalize(light.direction, Vector3{ 0.0f, -1.0f, 0.0f });

		// Centre the finite map on this game's own player so it follows the play area
		const Player* local = Client::LocalPlayer(*scene);
		const Vector3 focus = (local != nullptr) ? local->getPosition() : Vector3Zero();

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

		if (Render::BeginShadowPass(lightCamera))
		{
			DrawShadowCasters(scene);
			Render::EndShadowPass();
		}
	}
#pragma endregion

#pragma region Editor Gizmos
	void LightingSystem::DrawLightGizmo(const Light& light, bool highlighted, GizmoLayer layer) const
	{
		// A disabled light still draws, greyed out, so it can be found and switched
		// back on rather than becoming invisible level furniture.
		const Color bodyColor = light.isEnabled ? light.color : Color{ 110, 110, 110, 255 };
		const Color rayColor = Geometry::Fade(bodyColor, light.isEnabled ? 0.85f : 0.35f);
		const Color shapeColor = Geometry::Fade(bodyColor, light.isEnabled ? 0.40f : 0.18f);

		const Vector3 origin = light.position;
		const Vector3 direction = SafeNormalize(light.direction, Vector3{ 0.0f, -1.0f, 0.0f });
		const bool markers = (layer == GizmoLayer::Markers);

		if (markers)
		{
			// The position marker, in the light's own colour so a rig reads at a glance
			Render::DebugSphere(origin, GIZMO_BODY_RADIUS, 6, 8, bodyColor);

			// Selection halo drawn white, so it reads against any light colour
			if (highlighted) { Render::DebugSphereWires(origin, GIZMO_BODY_RADIUS * 2.2f, 8, 8, WHITE); }
		}

		switch (light.type)
		{
		case LIGHT_DIRECTIONAL:
		{
			// A directional light has no real position — the shader only uses its
			// direction — so `position` serves purely as the gizmo's anchor. A
			// bundle of parallel arrows says "this direction, everywhere".
			Vector3 right = {};
			Vector3 up = {};
			BuildBasis(direction, right, up);

			if (markers)
			{
				DrawArrow(origin, direction, GIZMO_SUN_RAY_LENGTH, 0.22f, rayColor);

				for (int i = 0; i < 4; i++)
				{
					const float angle = (2.0f * PI * static_cast<float>(i)) / 4.0f;
					const Vector3 offset = Vector3Add(
						Vector3Scale(right, cosf(angle) * GIZMO_SUN_SPREAD),
						Vector3Scale(up, sinf(angle) * GIZMO_SUN_SPREAD));

					DrawArrow(Vector3Add(origin, offset), direction, GIZMO_SUN_RAY_LENGTH, 0.14f, rayColor);
				}
			}
			else
			{
				// Ring binding the bundle together, so the anchor reads as one light
				DrawCircleFacing(origin, direction, GIZMO_SUN_SPREAD, shapeColor);
			}
			break;
		}

		case LIGHT_POINT:
		{
			if (markers)
			{
				// Spikes on all six axes — a point light has no single direction,
				// and showing that is the point of the indicator.
				const float spikeLength = fminf(light.range * 0.25f, 1.5f);
				const Vector3 axes[6] = {
					{ 1.0f, 0.0f, 0.0f },{ -1.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f, 0.0f },{ 0.0f, -1.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f },{ 0.0f, 0.0f, -1.0f }
				};

				for (const Vector3& axis : axes)
				{
					Render::DebugLine(origin, Vector3Add(origin, Vector3Scale(axis, spikeLength)), rayColor);
				}
			}
			else
			{
				// Three rings at `range`. Attenuation reaches exactly zero there,
				// so this is the light's true boundary, not an approximation.
				DrawCircleFacing(origin, Vector3{ 1.0f, 0.0f, 0.0f }, light.range, shapeColor, 32);
				DrawCircleFacing(origin, Vector3{ 0.0f, 1.0f, 0.0f }, light.range, shapeColor, 32);
				DrawCircleFacing(origin, Vector3{ 0.0f, 0.0f, 1.0f }, light.range, shapeColor, 32);
			}
			break;
		}

		case LIGHT_SPOT:
		{
			if (markers)
			{
				DrawArrow(origin, direction, light.range, 0.22f, rayColor);
				break;
			}

			const float outer = Clamp(light.outerConeDegrees, 0.0f, 89.9f);
			const float inner = Clamp(light.innerConeDegrees, 0.0f, outer);

			const Vector3 capCenter = Vector3Add(origin, Vector3Scale(direction, light.range));
			const float outerRadius = light.range * tanf(outer * DEG2RAD);
			const float innerRadius = light.range * tanf(inner * DEG2RAD);

			// Flat caps stand in for the true spherical cap (the shader compares
			// cosines, not distances) — the usual editor shorthand, and accurate
			// enough to aim a light with.
			DrawCircleFacing(capCenter, direction, outerRadius, shapeColor, 32);
			DrawCircleFacing(capCenter, direction, innerRadius, Geometry::Fade(bodyColor, 0.65f), 32);

			// Four edge lines give the cone a readable silhouette
			Vector3 right = {};
			Vector3 up = {};
			BuildBasis(direction, right, up);

			for (int i = 0; i < 4; i++)
			{
				const float angle = (2.0f * PI * static_cast<float>(i)) / 4.0f;
				const Vector3 edge = Vector3Add(capCenter,
					Vector3Add(Vector3Scale(right, cosf(angle) * outerRadius),
						Vector3Scale(up, sinf(angle) * outerRadius)));

				Render::DebugLine(origin, edge, shapeColor);
			}
			break;
		}

		default: break;
		}
	}

	void LightingSystem::DrawGizmos() const
	{
		if (!showGizmos) { return; }

		const int highlight = gizmoHighlightIndex;
		const int lightCount = static_cast<int>(lights.size());

		// Influence volumes are depth-tested so they read as sitting in the world,
		// occluded by the geometry they actually light. X-ray moves them into the
		// overlay pass below instead.
		if (!gizmoXray)
		{
			for (int i = 0; i < lightCount; i++)
			{
				DrawLightGizmo(lights[i], i == highlight, GizmoLayer::InfluenceShapes);
			}
		}

		// Overlay pass. Position markers and direction arrows always draw on top:
		// a light buried in a wall still has to be findable and aimable, which is
		// the entire point of the indicator.
		//
		// rlgl batches geometry and submits it later, so the batch has to be flushed
		// on both sides of the depth-test toggle — otherwise the state change lands
		// on the wrong draw calls and hides scene geometry instead.
		Render::SetDebugDepthTest(false);

		for (int i = 0; i < lightCount; i++)
		{
			if (gizmoXray) { DrawLightGizmo(lights[i], i == highlight, GizmoLayer::InfluenceShapes); }
			DrawLightGizmo(lights[i], i == highlight, GizmoLayer::Markers);
		}

		// The flashlight is deliberately skipped: it rides the active camera, so its
		// gizmo would be a solid blob jammed against the near plane every frame.

		Render::SetDebugDepthTest(true);
	}
#pragma endregion

#pragma region Settings & Save Data
	void LightingSystem::ApplySettings()
	{
		const auto& settings = Settings::Settings::getInstance();
		SetPlayerQuality(settings.Get<int>("Shadows"), settings.Get<float>("Luminosity"));
	}

	void LightingSystem::SetPlayerQuality(int shadowQuality, float luminosity)
	{
		// Off, Low, Medium, High, Ultra. A world asking for less than the cap
		// keeps its own size; quality never raises a map above what the world set.
		constexpr int limits[5] = { 0, 512, 1024, 2048, 4096 };
		playerShadowLimit = limits[std::clamp(shadowQuality, 0, 4)];
		playerExposure = std::max(luminosity, 0.0f);
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
		j["ShadowResolution"] = shadowResolution;

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

		// Kept exactly as saved, whatever the player's Shadows setting draws, so
		// saving this world again writes back what it was authored with
		SetShadowResolution(j.value("ShadowResolution", shadowResolution));
		shadowsEnabled = j.value("ShadowsEnabled", shadowsEnabled);

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

}
#pragma endregion

namespace SaveSystem
{
	SaveParticipant LightingParticipant()
	{
		return SaveParticipant{
			"Lighting",
			[] { return Lighting::LightingSystem::getInstance().formatToJson(); },
			[](const Json* section)
			{
				Lighting::LightingSystem& lighting = Lighting::LightingSystem::getInstance();
				if (section != nullptr) { lighting.loadFromJson(*section); }
				else { lighting.CreateDefaultRig(); }
			},
		};
	}
}
