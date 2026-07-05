#include <DeveloperWindow.h>

#include <WorldEditor.h>
void DeveloperWindow::ShowWorldEditor()
{
	if (!isWorldEditorActive) { return; }

	// Show World Editor Window
	ImGui::Begin("World Editor", &isWorldEditorActive);
	
	// Show World Editor Options
	ImGui::Text("World Editor Options");
	ImGui::Separator();
	
	// Show Grid Options
	ImGui::Checkbox("Show Grid", &WorldEditor::getInstance().isGridActive);
	ImGui::Checkbox("Snap to Grid", &WorldEditor::getInstance().isSnapActive);
	ImGui::SliderFloat("Grid Size", &WorldEditor::getInstance().gridSize, 0.1f, 10.0f);
	
	// Show Camera Options
	ImGui::Text("Camera Options");
	ImGui::Separator();
	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", WorldEditor::getInstance().editorCamera.position.x, WorldEditor::getInstance().editorCamera.position.y, WorldEditor::getInstance().editorCamera.position.z);
	ImGui::Text("Camera Target: (%.2f, %.2f, %.2f)", WorldEditor::getInstance().editorCamera.target.x, WorldEditor::getInstance().editorCamera.target.y, WorldEditor::getInstance().editorCamera.target.z);
	ImGui::Text("Camera Up: (%.2f, %.2f, %.2f)", WorldEditor::getInstance().editorCamera.up.x, WorldEditor::getInstance().editorCamera.up.y, WorldEditor::getInstance().editorCamera.up.z);
	ImGui::Text("Camera FOV: %.2f", WorldEditor::getInstance().editorCamera.fovy);
	
	// End Window
	ImGui::End();
}