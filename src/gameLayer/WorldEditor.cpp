#include "WorldEditor.h"

void WorldEditor::update(Player* player)
{
	if (PRODUCTION_BUILD == 1) { return; }

	if (IsKeyPressed(KEY_F1))
	{
		isEditorActive = !isEditorActive;

		// Adopt the camera the editor is taking over from. EditorCamera derives
		// from Camera3D and default-constructs its own position at the origin,
		// so without this the first F1 teleports the view to {0,0,0} and points
		// it at whatever the stale target happened to be.
		if (isEditorActive) { editorCamera.SyncFrom(SceneManager::getInstance().camera3D); }
		else { gizmo.Cancel(); placementMode = false; }
	}
	if (!isEditorActive) { return; } // Panel flags are remembered while hidden

	// Tools and panel shortcuts. Runs before the viewport so a tool change or a
	// delete takes effect on this frame's click rather than the next one.
	UpdateHotkeys();

	// Mouse arbitration: ImGui, camera, placement, gizmo, selection
	UpdateViewportInput();

	/// Update World Editor Windows
	ShowEditorHub();
	if (isWorldSettingsActive) ShowWorldSettings();
	if (isObjectBrowserActive) ShowObjectBrowser();
	if (isPlacementActive) ShowPlacementPanel();
	if (isPlayerActive) ShowPlayerData(player);
	if (isCameraActive) ShowCameraData(player);
	if (isMiniGameActive) ShowMiniGameData(player);
	if (isAssetActive) ShowAssetData();
	if (isLightingActive) ShowLightingData();
}

void WorldEditor::ShowEditorHub()
{
	ImGui::Begin("World Editor");

	showTransformTools();

	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Panels");
	ImGui::Checkbox("World Settings (Ctrl+1)", &isWorldSettingsActive);
	ImGui::Checkbox("Object Browser (Ctrl+2)", &isObjectBrowserActive);
	ImGui::Checkbox("Placement (Ctrl+3)", &isPlacementActive);
	ImGui::Checkbox("Player Data (Ctrl+4)", &isPlayerActive);
	ImGui::Checkbox("Camera Data (Ctrl+5)", &isCameraActive);
	ImGui::Checkbox("Mini Game Data (Ctrl+6)", &isMiniGameActive);
	ImGui::Checkbox("Asset Data (Ctrl+7)", &isAssetActive);
	ImGui::Checkbox("Lighting (Ctrl+8)", &isLightingActive);
	ImGui::Separator();

	// Mini Console
	if (!statusMessage.empty()) { ImGui::Text("%s", statusMessage.c_str()); }

	if (ImGui::Button("Close Editor (F1)"))
	{
		isEditorActive = false;
		gizmo.Cancel();
		placementMode = false;
	}

	ImGui::End();
}

/** Tool bar for the viewport manipulators — the front end for EditorGizmo. */
void WorldEditor::showTransformTools()
{
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Transform Tools");

	int mode = static_cast<int>(gizmo.mode);
	if (ImGui::RadioButton("Select (1)", &mode, GIZMO_SELECT)) { gizmo.mode = GIZMO_SELECT; }
	ImGui::SameLine();
	if (ImGui::RadioButton("Move (2)", &mode, GIZMO_TRANSLATE)) { gizmo.mode = GIZMO_TRANSLATE; }
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate (3)", &mode, GIZMO_ROTATE)) { gizmo.mode = GIZMO_ROTATE; }
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale (4)", &mode, GIZMO_SCALE)) { gizmo.mode = GIZMO_SCALE; }

	// Scale is always local, so the toggle would be a lie in that mode
	if (gizmo.mode == GIZMO_SCALE)
	{
		ImGui::TextDisabled("Space: Local (scale is always object-local)");
	}
	else
	{
		ImGui::Checkbox("Local Space (X)", &gizmo.localSpace);
	}

	ImGui::Checkbox("Snap (G)", &gizmo.snapEnabled);
	if (gizmo.snapEnabled)
	{
		ImGui::DragFloat("Move Step", &gizmo.translateSnap, 0.05f, 0.0f, 100.0f);
		ImGui::DragFloat("Rotate Step (deg)", &gizmo.rotateSnapDegrees, 1.0f, 0.0f, 180.0f);
		ImGui::DragFloat("Scale Step", &gizmo.scaleSnap, 0.05f, 0.0f, 100.0f);
	}

	// Editing against a live simulation means gravity drags placed objects away
	// and the solver fights every gizmo drag, so this defaults to paused
	ImGui::Checkbox("Pause Simulation", &simulationPaused);
	if (!simulationPaused)
	{
		ImGui::TextColored(ImVec4(255, 200, 0, 255), "Physics live: objects will move while edited");
	}

	if (ImGui::Button("Undo (Ctrl+Z)")) { Undo(); }
	ImGui::SameLine();
	if (ImGui::Button("Duplicate (Ctrl+D)")) { DuplicateSelection(); }
	ImGui::SameLine();
	if (ImGui::Button("Delete (Del)")) { DeleteSelection(); }

	if (ImGui::Button("Focus (F)")) { FocusOnSelection(); }
	ImGui::SameLine();
	if (ImGui::Button("Snap To Grid")) { SnapSelectionToGrid(); }

	ImGui::Text("History: %d", static_cast<int>(undoStack.size()));

	if (GameObject* selected = getSelectedObject())
	{
		ImGui::Text("Selected: %s (%llu)", selected->name.c_str(),
			static_cast<unsigned long long>(selected->id));
	}
	else
	{
		ImGui::TextDisabled("Nothing selected — left click an object");
	}

	ImGui::Separator();
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
	// The three lookups below all dereference currentScene without checking it.
	// This is now called from the render pass and from the hub every frame, not
	// only from panels that had already established a scene, so the guard lives
	// here rather than at each call site.
	if (SceneManager::getInstance().currentScene == nullptr) { return nullptr; }
	if (selectedObjectId == 0) { return nullptr; }

	if (InteractableObject* interactable = FindInteractableByID(selectedObjectId)) { return interactable; }
	if (Entity* entity = FindEntityByID(selectedObjectId)) { return entity; }
	return FindGameObjectByID(selectedObjectId);
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
