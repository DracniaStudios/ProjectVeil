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
		if (IsKeyPressed(KEY_FOUR)) { isPaletteActive = !isPaletteActive; }
	}

	/// Update World Editor Windows
	ShowEditorHub();
	if (isWorldSettingsActive) ShowWorldSettings();
	if (isObjectBrowserActive) ShowObjectBrowser();
	if (isPlacementActive) ShowPlacementPanel();
	if (isPaletteActive) ShowTexturePalette();
}

void WorldEditor::ShowEditorHub()
{
	ImGui::Begin("World Editor");

	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Panels");
	ImGui::Checkbox("World Settings (Ctrl+1)", &isWorldSettingsActive);
	ImGui::Checkbox("Object Browser (Ctrl+2)", &isObjectBrowserActive);
	ImGui::Checkbox("Placement (Ctrl+3)", &isPlacementActive);
	ImGui::Checkbox("Texture Palette (Ctrl+4)", &isPaletteActive);
	ImGui::Separator();

	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }

	if (ImGui::Button("Close Editor (F8)")) { isEditorActive = false; }

	ImGui::End();
}

GameObject* WorldEditor::getSelectedObject()
{
	if (selectedObjectId == 0) { return nullptr; }

	auto scene = SceneManager::getInstance().currentScene;
	for (auto& object : scene->gameMap.gameObjects)
	{
		if (object.id == selectedObjectId) { return &object; }
	}
	return nullptr;
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
