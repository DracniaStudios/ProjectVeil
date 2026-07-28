#include "WorldEditor.h"

void WorldEditor::update(Player* player)
{
	if (IsKeyPressed(KEY_F1)) { isEditorActive = !isEditorActive; }
	if (!isEditorActive) { return; } // Panel flags are remembered while hidden

	// Panel shortcuts, only read while the editor is active
	if (IsKeyDown(KEY_LEFT_CONTROL))
	{
		if (IsKeyPressed(KEY_ONE)) { isWorldSettingsActive = !isWorldSettingsActive; }
		if (IsKeyPressed(KEY_TWO)) { isObjectBrowserActive = !isObjectBrowserActive; }
		if (IsKeyPressed(KEY_THREE)) { isPlacementActive = !isPlacementActive; }
		if (IsKeyPressed(KEY_FOUR)) { isPlayerActive = !isPlayerActive; }
		if (IsKeyPressed(KEY_FIVE)) { isCameraActive = !isCameraActive; }
		if (IsKeyPressed(KEY_SIX)) { isMiniGameActive = !isMiniGameActive; }
		if (IsKeyPressed(KEY_SEVEN)) { isAssetActive = !isAssetActive; }
	}

	// Developer tool shortcuts, kept from the standalone Developer Window

	/// Update World Editor Windows
	ShowEditorHub();
	if (isWorldSettingsActive) ShowWorldSettings();
	if (isObjectBrowserActive) ShowObjectBrowser();
	if (isPlacementActive) ShowPlacementPanel();
	if (isPlayerActive) ShowPlayerData(player);
	if (isCameraActive) ShowCameraData(player);
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
	ImGui::Checkbox("Player Data (Ctrl+4)", &isPlayerActive);
	ImGui::Checkbox("Camera Data (Ctrl+5)", &isCameraActive);
	ImGui::Checkbox("Mini Game Data (Ctrl+6)", &isMiniGameActive);
	ImGui::Checkbox("Asset Data (Ctrl+7)", &isAssetActive);
	ImGui::Separator();

	// Mini Console 
	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }

	if (ImGui::Button("Close Editor (F1)")) { isEditorActive = false; }

	ImGui::End();
}

GameObject* FindGameObjectByID(uint64_t id)
{
	const auto scene = SceneManager::getInstance().currentScene;
	for (size_t i = 0; i < scene->gameMap.gameObjects.size(); ++i) {
		auto obj = &scene->gameMap.gameObjects[i];
		if (obj->id == id) { return obj; }
	}
	return nullptr;
};

Entity* FindEntityByID(uint64_t id)
{
	const auto scene = SceneManager::getInstance().currentScene;
	auto it = scene->entities.find(id);
	if (it == scene->entities.end()) { return nullptr; }
	return it->second.get();
};

InteractableObject* FindInteractableByID(uint64_t id)
{
	const auto scene = SceneManager::getInstance().currentScene;
	auto it = scene->interactables.find(id);
	if (it == scene->interactables.end()) { return nullptr; }
	return it->second.get();
};

GameObject* WorldEditor::getSelectedObject()
{
	return
		FindInteractableByID(selectedObjectId) != nullptr ? FindInteractableByID(selectedObjectId) :
		FindEntityByID(selectedObjectId) != nullptr ? FindEntityByID(selectedObjectId) :
		FindGameObjectByID(selectedObjectId);
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
