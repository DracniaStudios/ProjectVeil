#include <WorldEditor.h>

void WorldEditor::ShowAssetData()
{
	auto& assetManager = AssetManager::getInstance();
	auto& assets = assetManager.assets;
	ImGui::SetNextWindowSize({ 640, 480 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints({ 0, 0 }, { FLT_MAX, FLT_MAX });

	if (ImGui::Begin("Asset Data", &isAssetActive, 0)) {
		ImGui::Text("Total Assets: %zu", assetManager.assets.size());
		ImGui::Separator();

		// Add Draw Calls of dependent popup wiindows here

		if (ImGui::BeginTabBar("Assets", 0)) {
			
			/// Create a Tab for Textures
			if (ImGui::BeginTabItem("Textures", nullptr, ImGuiTabItemFlags_None)) {

				Asset* activeTexture = getActiveTexture();
				ImGui::TextColored(ImVec4(0, 255, 0, 255), "Active: %s", activeTexture != nullptr ? activeTexture->name.c_str() : "None");
				ImGui::Separator();

				/// Thumbnail Grid
				const float thumbSize = 64.0f;
				const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (thumbSize + 12.0f)));

				for (int i = 0; i < static_cast<int>(assets.size()); i++)
				{
					if (assets[i].type != ASSET_TEXTURE) continue;
					ImGui::PushID(i);

					bool isSelected = (i == activeTextureIndex);
					if (isSelected) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); }

					// Models have no thumbnail texture — show a labeled button instead
					bool clicked = ImGui::ImageButton("##texture", (ImTextureRef)(intptr_t)assets[i].texture.id, ImVec2(thumbSize, thumbSize));
					if (clicked)
					{
						if (assets[i].type == ASSET_TEXTURE) activeTextureIndex = i;
					}

					if (isSelected) { ImGui::PopStyleColor(); }
					if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", assets[i].name.c_str()); }

					if ((i + 1) % columns != 0) { ImGui::SameLine(); }

					ImGui::PopID();
				}

			ImGui::EndTabItem();
			}

			/// Create a Tab for Models
			if (ImGui::BeginTabItem("Models", nullptr, ImGuiTabItemFlags_None)) {

				Asset* activeModel = getActiveModel();
				ImGui::TextColored(ImVec4(0, 255, 0, 255), "Active: %s", activeModel != nullptr ? activeModel->name.c_str() : "None");
				ImGui::Separator();

				/// Thumbnail Grid
				const float thumbSize = 64.0f;
				const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (thumbSize + 12.0f)));

				for (int i = 0; i < static_cast<int>(assets.size()); i++)
				{
					if (assets[i].type != ASSET_MODEL) continue;
					ImGui::PushID(i);

					bool isSelected = (i == activeModelIndex);
					if (isSelected) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); }

					// Models have no thumbnail texture — show a labeled button instead
					// To Display Models check ImGui #475 issue on GitHub
					bool clicked = ImGui::Button(assets[i].name.c_str(), ImVec2(thumbSize, thumbSize));
					if (clicked)
					{
						if (assets[i].type == ASSET_MODEL) activeModelIndex = i;
					}

					if (isSelected) { ImGui::PopStyleColor(); }
					if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", assets[i].name.c_str()); }

					if ((i + 1) % columns != 0) { ImGui::SameLine(); }

					ImGui::PopID();
				}

			ImGui::EndTabItem();
			}

			/// Create A Tab for each folder in the asset manager
			for (auto& folder : assetManager.folders)
			{
				// EndTabItem() must only be called when BeginTabItem() returns true —
				// calling it unconditionally pops an ID that was never pushed for
				// inactive tabs, corrupting ImGui's ID stack for the rest of the frame.
				if (ImGui::BeginTabItem(folder.c_str(), nullptr, ImGuiTabItemFlags_None)) {

				Asset* activeAsset = getActiveAsset();
				ImGui::TextColored(ImVec4(0, 255, 0, 255), "Active: %s", activeAsset != nullptr ? activeAsset->name.c_str() : "None");
				ImGui::Separator();

				/// Thumbnail Grid
				const float thumbSize = 64.0f;
				const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (thumbSize + 12.0f)));

				// Check Asset Lists for the folder and display them
				for (int i = 0; i < static_cast<int>(assets.size()); i++)
				{
					const auto& asset = assets[i];
					if (asset.folder != folder) { continue; }
					ImGui::PushID(i);
					ImGui::Text("%s (%s)", asset.name.c_str(), asset.type == ASSET_MODEL ? "Model" : "Texture");

					bool isSelected = (i == activeAssetIndex); /// Disabled for now, as we don't have a way to select assets in this view yet
					if (isSelected) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); }

					// Models have no thumbnail texture — show a labeled button instead
					// To Display Models check ImGui #475 issue on GitHub
					bool clicked = ImGui::Button(assets[i].name.c_str(), ImVec2(thumbSize, thumbSize));
					if (clicked)
					{
						if (assets[i].type == ASSET_TEXTURE) activeAssetIndex = i;
					}

					if (isSelected) { ImGui::PopStyleColor(); }
					if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", assets[i].name.c_str()); }

					if ((i + 1) % columns != 0) { ImGui::SameLine(); }

					ImGui::PopID();
				}

				ImGui::EndTabItem();
				}
			}

		ImGui::EndTabBar();
		}
	}
	ImGui::End();
}