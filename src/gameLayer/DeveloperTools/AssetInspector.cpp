#include <DeveloperWindow.h>

void DeveloperWindow::ShowAssetData()
{
	auto& assetManager = AssetManager::getInstance();
	ImGui::Begin("Asset Data");
	ImGui::Text("Total Assets: %d", assetManager.assets.size());
	ImGui::Separator();

	for (const auto& asset : assetManager.assets)
	{
		if (ImGui::TreeNode(asset.name.c_str()))
		{
			ImGui::Text("Asset Name: %s", asset.name.c_str());
			ImGui::Text("Asset Path: %s", asset.path.c_str());
			ImGui::Text("TextureID: %s", std::to_string(asset.texture.id).c_str());
			ImGui::Image((ImTextureRef)(intptr_t)asset.texture.id, ImVec2(64, 64));
			ImGui::TreePop();
		}
		ImGui::Separator();
	}
	ImGui::End();
}