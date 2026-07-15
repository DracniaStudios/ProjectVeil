#include "WorldEditor.h"

void WorldEditor::ShowTexturePalette()
{
	auto& assets = AssetManager::getInstance().assets;

	/// Show Texture Palette Window
	ImGui::Begin("Texture Palette");

	Asset* activeTexture = getActiveTexture();
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Active: %s", activeTexture != nullptr ? activeTexture->name.c_str() : "None");
	ImGui::Separator();

	/// Thumbnail Grid
	const float thumbSize = 64.0f;
	const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (thumbSize + 12.0f)));

	for (int i = 0; i < static_cast<int>(assets.size()); i++)
	{
		ImGui::PushID(i);

		bool isSelected = (i == activeTextureIndex) || (i == activeModelIndex);
		if (isSelected) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); }

		// Models have no thumbnail texture — show a labeled button instead
		bool clicked = assets[i].type == ASSET_MODEL
			? ImGui::Button(assets[i].name.c_str(), ImVec2(thumbSize, thumbSize))
			: ImGui::ImageButton("##texture", (ImTextureRef)(intptr_t)assets[i].texture.id, ImVec2(thumbSize, thumbSize));
		if (clicked)
		{
			if (assets[i].type == ASSET_TEXTURE) activeTextureIndex = i;
			if (assets[i].type == ASSET_MODEL) activeModelIndex = i;
		}

		if (isSelected) { ImGui::PopStyleColor(); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", assets[i].name.c_str()); }

		if ((i + 1) % columns != 0) { ImGui::SameLine(); }

		ImGui::PopID();
	}

	ImGui::End();
}
