#include "WorldEditor.h"

#include <LightingSystem.h>

#include <cstdio>

namespace
{
	// ImGui edits colours as normalized floats; every authored colour in Veil is
	// a raylib Color. These two keep the conversion in one place.
	void ColorToFloat3(const Color& color, float* out)
	{
		out[0] = color.r / 255.0f;
		out[1] = color.g / 255.0f;
		out[2] = color.b / 255.0f;
	}

	void Float3ToColor(const float* values, Color& color)
	{
		color.r = static_cast<unsigned char>(Clamp(values[0], 0.0f, 1.0f) * 255.0f);
		color.g = static_cast<unsigned char>(Clamp(values[1], 0.0f, 1.0f) * 255.0f);
		color.b = static_cast<unsigned char>(Clamp(values[2], 0.0f, 1.0f) * 255.0f);
	}

	// Returns true when the user changed the colour this frame
	bool EditColor(const char* label, Color& color)
	{
		float values[3] = {};
		ColorToFloat3(color, values);
		if (!ImGui::ColorEdit3(label, values)) { return false; }
		Float3ToColor(values, color);
		return true;
	}

	// Shadow map sizes offered in the panel, mirroring the Video > Shadows
	// quality steps in Settings
	constexpr int SHADOW_RESOLUTIONS[4] = { 512, 1024, 2048, 4096 };
	constexpr const char* SHADOW_RESOLUTION_LABELS[4] = { "512", "1024", "2048", "4096" };

	int ResolutionToIndex(int resolution)
	{
		for (int i = 0; i < 4; i++)
		{
			if (SHADOW_RESOLUTIONS[i] == resolution) { return i; }
		}
		return 2; // 2048
	}
}

void WorldEditor::ShowLightingData()
{
	auto lighting = &LightingSystem::getInstance();
	auto& lights = lighting->GetLights();

	ImGui::Begin("Lighting");

	/// Diagnostics — the first thing to check when the scene renders unlit
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Status");
	if (!lighting->IsReady())
	{
		ImGui::TextColored(ImVec4(255, 0, 0, 255), "Shader failed to load - scene is unlit");
		ImGui::Text("Expected: resources/shaders/glsl330/lighting.vs + .fs");
		ImGui::End();
		return;
	}
	ImGui::Text("Shader ID: %u", lighting->GetShader().id);
	ImGui::Text("Active Lights: %d / %d", lighting->GetActiveLightCount(), LIGHT_LIMIT);
	ImGui::Text("Shadow Map: %dx%d", lighting->GetShadowMapResolution(), lighting->GetShadowMapResolution());
	const int shadowOwner = lighting->GetShadowLightIndex();
	ImGui::Text("Shadow Caster: %s",
		(shadowOwner >= 0 && shadowOwner < static_cast<int>(lights.size()))
			? lights[shadowOwner].name.c_str()
			: "None");
	ImGui::Separator();

	/// Atmosphere
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Atmosphere");
	EditColor("Ambient Color", lighting->ambientColor);
	ImGui::SliderFloat("Ambient Strength", &lighting->ambientStrength, 0.0f, 1.0f);

	// Changing the fog colour also changes the backbuffer clear colour, since
	// update_game() clears to it — that is what keeps distance from fading
	// toward a bright wall.
	EditColor("Fog Color", lighting->fogColor);
	ImGui::SliderFloat("Fog Density", &lighting->fogDensity, 0.0f, 0.25f, "%.4f");

	ImGui::SliderFloat("Exposure", &lighting->exposure, 0.0f, 3.0f);
	ImGui::SliderFloat("Specular Strength", &lighting->specularStrength, 0.0f, 2.0f);
	ImGui::SliderFloat("Shininess", &lighting->shininess, 1.0f, 128.0f);
	ImGui::Separator();

	/// Shadows
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Shadows");
	ImGui::Checkbox("Shadows Enabled", &lighting->shadowsEnabled);

	int resolutionIndex = ResolutionToIndex(lighting->GetShadowMapResolution());
	if (ImGui::Combo("Resolution", &resolutionIndex, SHADOW_RESOLUTION_LABELS, 4))
	{
		// Reallocates the depth framebuffer now when shadows are on; when they
		// are off the size is remembered and allocated on the next enable
		statusMessage = lighting->ResizeShadowMap(SHADOW_RESOLUTIONS[resolutionIndex])
			? "Shadow map set to " + std::string(SHADOW_RESOLUTION_LABELS[resolutionIndex])
			: "Failed to allocate shadow map";
	}

	// Bias trades shadow acne (too low) against peter-panning (too high)
	ImGui::SliderFloat("Bias", &lighting->shadowBias, 0.0f, 0.02f, "%.5f");
	// The orthographic extent of the caster camera: smaller = sharper, but the
	// shadowed area around the player shrinks
	ImGui::SliderFloat("Distance", &lighting->shadowDistance, 10.0f, 250.0f);
	ImGui::SliderFloat("Softness", &lighting->shadowSoftness, 0.0f, 4.0f);

	if (ImGui::Button("Reload Quality From Settings")) { lighting->ApplySettings(); }
	ImGui::Separator();

	/// Flashlight
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Flashlight");
	bool flashlightOn = lighting->IsFlashlightEnabled();
	if (ImGui::Checkbox("Flashlight On (L)", &flashlightOn)) { lighting->SetFlashlightEnabled(flashlightOn); }

	Light& flashlight = lighting->GetFlashlight();
	EditColor("Beam Color", flashlight.color);
	ImGui::SliderFloat("Beam Intensity", &flashlight.intensity, 0.0f, 5.0f);
	ImGui::SliderFloat("Beam Range", &flashlight.range, 1.0f, 120.0f);
	ImGui::SliderFloat("Beam Inner Cone", &flashlight.innerConeDegrees, 0.0f, 89.0f);
	ImGui::SliderFloat("Beam Outer Cone", &flashlight.outerConeDegrees, 0.0f, 89.9f);
	ImGui::Separator();

	/// World Light List
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "World Lights");

	if (ImGui::Button("Add Point Light At Camera"))
	{
		if (static_cast<int>(lights.size()) >= LIGHT_LIMIT)
		{
			statusMessage = "Light limit reached (" + std::to_string(LIGHT_LIMIT) + ")";
		}
		else
		{
			Light light = {};
			light.name = "Point Light " + std::to_string(lights.size() + 1);
			light.type = LIGHT_POINT;
			light.position = SceneManager::getInstance().camera3D.position;
			lighting->AddLight(light);
			selectedLightIndex = static_cast<int>(lights.size()) - 1;
			statusMessage = "Added " + lights.back().name;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Directional"))
	{
		if (static_cast<int>(lights.size()) >= LIGHT_LIMIT)
		{
			statusMessage = "Light limit reached (" + std::to_string(LIGHT_LIMIT) + ")";
		}
		else
		{
			Light light = {};
			light.name = "Directional Light " + std::to_string(lights.size() + 1);
			light.type = LIGHT_DIRECTIONAL;
			light.intensity = 0.6f;
			lighting->AddLight(light);
			selectedLightIndex = static_cast<int>(lights.size()) - 1;
			statusMessage = "Added " + lights.back().name;
		}
	}

	if (ImGui::Button("Reset To Default Rig"))
	{
		lighting->CreateDefaultRig();
		selectedLightIndex = -1;
		statusMessage = "Installed the default lighting rig";
	}
	ImGui::SameLine();
	if (ImGui::Button("Reapply Shader To Scene"))
	{
		// Safety net: rebinds the lighting shader onto every material in the
		// live scene, for objects created while the system was still down
		lighting->ApplyToScene(SceneManager::getInstance().currentScene);
		statusMessage = "Reapplied the lighting shader";
	}

	if (static_cast<int>(lights.size()) > LIGHT_LIMIT)
	{
		ImGui::TextColored(ImVec4(255, 255, 0, 255),
			"%d lights defined - only the first %d enabled ones are uploaded",
			static_cast<int>(lights.size()), LIGHT_LIMIT);
	}

	ImGui::BeginChild("Light List", ImVec2(ImGui::GetContentRegionAvail().x, 140.0f));
	for (int i = 0; i < static_cast<int>(lights.size()); i++)
	{
		ImGui::PushID(i);
		std::string label = lights[i].name + " [" + lightTypeToString(lights[i].type) + "]";
		if (!lights[i].isEnabled) { label += " (off)"; }
		if (ImGui::Selectable(label.c_str(), i == selectedLightIndex)) { selectedLightIndex = i; }
		ImGui::PopID();
	}
	ImGui::EndChild();
	ImGui::Separator();

	// Re-validated every frame: a world load or a delete can shrink the list
	// out from under the stored index.
	if (selectedLightIndex < 0 || selectedLightIndex >= static_cast<int>(lights.size()))
	{
		ImGui::Text("No light selected");
		if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }
		ImGui::End();
		return;
	}

	/// Selected Light
	Light& light = lights[selectedLightIndex];
	ImGui::PushID(selectedLightIndex);
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Selected Light");

	char nameBuffer[128] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", light.name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) { light.name = nameBuffer; }

	int typeIndex = static_cast<int>(light.type);
	const char* typeLabels[LIGHT_TYPE_COUNT] = { "Directional", "Point", "Spot" };
	if (ImGui::Combo("Type", &typeIndex, typeLabels, LIGHT_TYPE_COUNT))
	{
		light.type = static_cast<LightType>(typeIndex);
	}

	ImGui::Checkbox("Enabled", &light.isEnabled);

	// Only one directional light can own the single shadow map, so enabling it
	// here clears the flag on every other light rather than silently losing.
	bool castsShadow = light.castsShadow;
	if (ImGui::Checkbox("Casts Shadow (directional only)", &castsShadow))
	{
		light.castsShadow = castsShadow;
		if (castsShadow)
		{
			for (int i = 0; i < static_cast<int>(lights.size()); i++)
			{
				if (i != selectedLightIndex) { lights[i].castsShadow = false; }
			}
			statusMessage = (light.type == LIGHT_DIRECTIONAL)
				? light.name + " now owns the shadow map"
				: "Only directional lights can cast shadows";
		}
	}

	// Position is meaningless for a directional light (it is at infinity), and
	// direction is meaningless for a point light.
	if (light.type != LIGHT_DIRECTIONAL) { ImGui::InputFloat3("Position", &light.position.x); }
	if (light.type != LIGHT_POINT) { ImGui::InputFloat3("Direction", &light.direction.x); }

	if (ImGui::Button("Move To Camera"))
	{
		const auto camera = &SceneManager::getInstance().camera3D;
		light.position = camera->position;
		light.direction = Vector3Subtract(camera->target, camera->position);
	}

	EditColor("Color", light.color);
	ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 5.0f);

	if (light.type != LIGHT_DIRECTIONAL)
	{
		// Attenuation reaches exactly zero at `range`, so this is a real
		// authoring number rather than a physical falloff constant
		ImGui::SliderFloat("Range", &light.range, 0.5f, 200.0f);
	}

	if (light.type == LIGHT_SPOT)
	{
		ImGui::SliderFloat("Inner Cone", &light.innerConeDegrees, 0.0f, 89.0f);
		ImGui::SliderFloat("Outer Cone", &light.outerConeDegrees, 0.0f, 89.9f);
	}

	// Snapshotted before the mutating buttons below: AddLight() can reallocate
	// the vector, which leaves `light` dangling for anything that follows.
	const std::string selectedName = light.name;

	if (ImGui::Button("Duplicate Light"))
	{
		if (static_cast<int>(lights.size()) >= LIGHT_LIMIT)
		{
			statusMessage = "Light limit reached (" + std::to_string(LIGHT_LIMIT) + ")";
		}
		else
		{
			// Copy by value first for the same reason
			Light copy = light;
			copy.name += " Copy";
			copy.castsShadow = false; // Only one light may own the shadow map
			lighting->AddLight(copy);
			selectedLightIndex = static_cast<int>(lights.size()) - 1;
			statusMessage = "Duplicated " + selectedName;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete Light"))
	{
		lighting->RemoveLight(selectedLightIndex);
		selectedLightIndex = -1;
		statusMessage = "Deleted " + selectedName;
	}

	ImGui::PopID();

	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }
	ImGui::End();
}
