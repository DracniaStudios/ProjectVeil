#include "WorldEditor.h"

#include <SaveSystem.h>

void WorldEditor::ShowObjectBrowser()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show Object Browser Window
	ImGui::Begin("Object Browser");

	/// List World Objects
	ImGui::BeginChild("World Object List", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.3f));
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "World Objects");
	for (auto& object : scene->gameMap.gameObjects)
	{
		if (getType(&object) == OBJECT_PROJECTILE) continue;
		if (getType(&object) == OBJECT_PLAYER) continue;

		ImGui::PushID(&object);
		std::string label = object.name + " (" + std::to_string(object.id) + ")";
		if (ImGui::Selectable(label.c_str(), object.id == selectedObjectId))
		{
			selectedObjectId = object.id;
		}
		ImGui::PopID();
	}
	ImGui::EndChild();
	ImGui::Separator();

	/// Show Selected Object Data
	ImGui::BeginChild("Selected Object");

	GameObject* object = getSelectedObject();
	if (object == nullptr)
	{
		ImGui::Text("Nothing selected");
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	ImGui::PushID(object);

	std::string dataString = objectTypeToString(object->type);
	dataString += " Data";
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "%s", dataString.c_str());
	ImGui::Text("Name: %s", object->name.c_str());
	ImGui::Text("ID: %llu", static_cast<unsigned long long>(object->id));
	ImGui::Spacing();

	// Transform (Teleport keeps the collision box in sync with the new position)
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Transform");
	Vector3 position = object->getPosition();
	if (ImGui::InputFloat3("Position: ", &position.x))
	{
		object->rigidBody3D.Teleport(position);
	}
	ImGui::InputFloat3("Scale: ", &object->rigidBody3D.scale.x);
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SaveSystem::RestoreVisuals(*object); // Regenerate the cube mesh at the new scale
	}
	ImGui::Spacing();

	// Texture
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Texture");
	ImGui::Image((ImTextureRef)(intptr_t)object->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id, ImVec2(64, 64));
	Asset* activeTexture = getActiveTexture();
	if (activeTexture != nullptr && ImGui::Button("Apply Active Texture"))
	{
		object->texture = activeTexture;
		SaveSystem::RestoreVisuals(*object);
	}
	ImGui::Spacing();

	// Object Flags
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
	ImGui::Checkbox("isEnabled", &object->rigidBody3D.isEnabled);
	ImGui::Checkbox("isStatic", &object->rigidBody3D.isStatic);
	ImGui::Checkbox("isVisible", &object->display3DModel);
	ImGui::Checkbox("isDestructible", &object->isDestructible);
	ImGui::Checkbox("Show Collider", &object->displayCollider);
	ImGui::Separator();

	if (ImGui::Button("Duplicate Object"))
	{
		GameObject copy = *object;
		copy.rigidBody3D.Teleport(Vector3Add(copy.getPosition(), Vector3(1, 0, 0)));

		// saveObject may reallocate gameObjects — object is stale after this call
		GameObject* spawned = scene->gameMap.saveObject(copy);
		SaveSystem::RestoreVisuals(*spawned); // Give the copy its own model instead of sharing meshes
		selectedObjectId = spawned->id;
		statusMessage = "Duplicated: " + spawned->name;
	}
	else if (ImGui::Button("Delete Object"))
	{
		if (scene->interactables.contains(object->id))
		{
			scene->gameMap.removeInteractable(scene->interactables[object->id].get());
		}
		else
		{
			scene->gameMap.removeObject(object);
		}
		selectedObjectId = 0;
	}

	ImGui::PopID();

	ImGui::EndChild();
	ImGui::End();
}
