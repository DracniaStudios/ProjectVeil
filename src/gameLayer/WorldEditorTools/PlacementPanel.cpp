#include "WorldEditor.h"

void WorldEditor::ShowPlacementPanel()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show Placement Window
	ImGui::Begin("Placement");

	/** Base Object Data **/
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "New Object");
	ImGui::InputText("Name: ", inputName, 128);
	{
		stagingObject.name = inputName;
	}
	ImGui::InputInt("Object Type: ", &stagingObject.type, 1, 1);
	stagingObject.type = Clamp(stagingObject.type, 0, OBJECT_COUNT - 1);
	ImGui::Text("%s", objectTypeToString(stagingObject.type));

	// Color ( float to unsigned char conversion )
	{
		ImGui::InputFloat4("Color", &colorHolder.x);
		stagingObject.defaultColor = Color(
			static_cast<unsigned char>(Clamp(colorHolder.x, 0, 255)),
			static_cast<unsigned char>(Clamp(colorHolder.y, 0, 255)),
			static_cast<unsigned char>(Clamp(colorHolder.z, 0, 255)),
			static_cast<unsigned char>(Clamp(colorHolder.w, 0, 255))
		);
	}

	// Transform Data
	ImGui::InputFloat3("Position: ", &stagingObject.rigidBody3D.translation.x);
	ImGui::InputFloat3("Scale: ", &stagingObject.rigidBody3D.scale.x);
	ImGui::Spacing();

	// Active Texture from the Palette
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Texture");
	Asset* activeTexture = getActiveTexture();
	if (activeTexture != nullptr)
	{
		ImGui::Text("Active: %s", activeTexture->name.c_str());
		ImGui::Image((ImTextureRef)(intptr_t)activeTexture->texture.id, ImVec2(64, 64));
		if (ImGui::Button("Clear Texture")) { activeTextureIndex = -1; }
	}
	else
	{
		ImGui::Text("No texture selected (open the Texture Palette)");
	}
	ImGui::Spacing();

	// Active Model from the Palette
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Model");
	Asset* activeModel = getActiveModel();
	if (activeModel != nullptr)
	{
		ImGui::Text("Active: %s", activeModel->name.c_str());
		if (ImGui::Button("Clear Model")) { activeModelIndex = -1; }
	}
	else
	{
		ImGui::Text("No model selected (open the Texture Palette)");
	}
	ImGui::Spacing();

	// Object Flags
	ImGui::TextColored(ImVec4(100, 0, 0, 255), "Flags");
	ImGui::Checkbox("isEnabled", &stagingObject.rigidBody3D.isEnabled);
	ImGui::Checkbox("isStatic", &stagingObject.rigidBody3D.isStatic);
	ImGui::Checkbox("isVisible", &stagingObject.display3DModel);
	ImGui::Checkbox("isDestructible", &stagingObject.isDestructible);
	ImGui::Checkbox("Show Collider", &stagingObject.displayCollider);
	ImGui::Separator();

	// Spawn a copy so the staging object stays around for repeat placement
	auto spawnAt = [&](Vector3 position)
		{
			GameObject object = stagingObject;

			// Record assets by name — saveObject() binds them via onEnable()
			if (Asset* texture = getActiveTexture()) { object.textureName = texture->name; }
			if (Asset* model = getActiveModel()) { object.modelName = model->name; }

			object.rigidBody3D.translation = position;
			object.rigidBody3D.Teleport(position);

			GameObject* spawned = scene->gameMap.saveObject(object);
			selectedObjectId = spawned->id;
			statusMessage = "Spawned: " + spawned->name;
		};

	if (ImGui::Button("Spawn Game Object")) { spawnAt(stagingObject.getPosition()); }
	if (scene->player != nullptr && scene->camera != nullptr && ImGui::Button("Spawn At Player"))
	{
		Vector3 position = Vector3Add(
			scene->player->rigidBody3D.translation,
			Vector3Scale(scene->camera->forward, 3.0f)
		);
		spawnAt(position);
	}

	ImGui::End();
}
