#include "WorldEditor.h"

void WorldEditor::update(Player* player)
{
	if (IsKeyPressed(KEY_F8)) { isEditorActive = !isEditorActive; }
	if (!isEditorActive) { return; } // Panel flags are remembered while hidden

	// Panel shortcuts, only read while the editor is active
	if (IsKeyDown(KEY_LEFT_CONTROL))
	{
		if (IsKeyPressed(KEY_ONE)) { isWorldSettingsActive = !isWorldSettingsActive; }
		if (IsKeyPressed(KEY_TWO)) { isObjectBrowserActive = !isObjectBrowserActive; }
		if (IsKeyPressed(KEY_THREE)) { isPlacementActive = !isPlacementActive; }
	}

	// Developer tool shortcuts, kept from the standalone Developer Window
	if (IsKeyPressed(KEY_F2)) { isPlayerActive = !isPlayerActive; }
	if (IsKeyPressed(KEY_F3)) { isCameraActive = !isCameraActive; }
	if (IsKeyPressed(KEY_F5)) { isMiniGameActive = !isMiniGameActive; }
	if (IsKeyPressed(KEY_F6)) { isEntityActive = !isEntityActive; }
	if (IsKeyPressed(KEY_F7)) { isAssetActive = !isAssetActive; }

	/// Update World Editor Windows
	ShowEditorHub();
	if (isWorldSettingsActive) ShowWorldSettings();
	if (isObjectBrowserActive) ShowObjectBrowser();
	if (isPlacementActive) ShowPlacementPanel();

	/// Update Developer Tool Windows
	if (isPlayerActive) ShowPlayerData(player);
	if (isCameraActive) ShowCameraData(player);
	if (isEntityActive) ShowEntityInspector();
	if (isMiniGameActive) ShowMiniGameData(player);
	if (isAssetActive) ShowAssetData();
}

void WorldEditor::ShowEditorHub()
{
	ImGui::Begin("World Editor");

	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Panels");
	ImGui::Checkbox("World Settings (Ctrl+1)", &isWorldSettingsActive);
	ImGui::Checkbox("Object Browser (Ctrl+2)", &isObjectBrowserActive);
	ImGui::Checkbox("Placement (Ctrl+3)", &isPlacementActive);
	ImGui::Separator();

	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Developer Tools");
	ImGui::Checkbox("Player Data (F2)", &isPlayerActive);
	ImGui::Checkbox("Camera Data (F3)", &isCameraActive);
	ImGui::Checkbox("Mini Game Data (F5)", &isMiniGameActive);
	ImGui::Checkbox("Entity Inspector (F6)", &isEntityActive);
	ImGui::Checkbox("Asset Data (F7)", &isAssetActive);
	ImGui::Separator();

	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }

	if (ImGui::Button("Close Editor (F8)")) { isEditorActive = false; }

	ImGui::End();
}

GameObject* WorldEditor::findGameObject(std::uint64_t id)
{
	if (id == 0) { return nullptr; }

	auto scene = SceneManager::getInstance().currentScene;
	for (auto& object : scene->gameMap.gameObjects)
	{
		if (object.id == id) { return &object; }
	}
	return nullptr;
}

Entity* WorldEditor::findEntity(std::uint64_t id)
{
	if (id == 0) { return nullptr; }

	auto& entities = SceneManager::getInstance().currentScene->entities;
	auto entity = entities.find(id);
	if (entity == entities.end()) { return nullptr; }
	return entity->second.get();
}

GameObject* WorldEditor::getSelectedObject()
{
	return findGameObject(selectedObjectId);
}

Asset* WorldEditor::getActiveTexture()
{
	auto& assets = AssetManager::getInstance().assets;
	if (activeTextureIndex < 0 || activeTextureIndex >= static_cast<int>(assets.size())) { return nullptr; }
	return &assets[activeTextureIndex];
}

Asset* WorldEditor::getActiveModel()
{
	auto& assets = AssetManager::getInstance().assets;
	if (activeModelIndex < 0 || activeModelIndex >= static_cast<int>(assets.size())) { return nullptr; }
	return &assets[activeModelIndex];
}
