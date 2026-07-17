#include "WorldEditor.h"

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
	ImGui::Text("Type: %d", static_cast<int>(object->type));
	ImGui::Spacing();


	// Transform (Teleport keeps the collision box in sync with the new position)
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Transform");
	Vector3 position = object->getPosition();
	if (ImGui::InputFloat3("Position: ", &position.x))
	{
		object->rigidBody3D.Teleport(position);
	}
	Vector4 rotation = object->getRotation();
	if (ImGui::InputFloat4("Rotaton: ", &rotation.x))
	{
		object->rigidBody3D.rotation = rotation;
	}
	// Scale is applied through the model transform each frame — no mesh regen needed
	ImGui::InputFloat3("Scale: ", &object->rigidBody3D.scale.x);
	ImGui::Spacing();

	// RigidBody
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "RigidBody");
	Vector3 velocity = object->rigidBody3D.GetVelocity();
	if (ImGui::InputFloat3("Velocity: ", &velocity.x))
	{
		object->rigidBody3D.velocity = velocity;
	}
	Vector3 acceleration = object->rigidBody3D.GetAcceleration();
	if (ImGui::InputFloat3("Acceleration: ", &acceleration.x))
	{
		object->rigidBody3D.acceleration = acceleration;
	}
	Vector3 angularVelocity = object->rigidBody3D.angularVelocity;
	if (ImGui::InputFloat3("Angular Velocity: ", &angularVelocity.x))
	{
		object->rigidBody3D.angularVelocity = angularVelocity;
	}
	ImGui::Spacing();

	// Texture
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Texture");
	ImGui::Image((ImTextureRef)(intptr_t)object->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id, ImVec2(64, 64));
	ImGui::Text("Texture: %s", object->textureName.empty() ? "None" : object->textureName.c_str());
	Asset* activeTexture = getActiveTexture();
	if (activeTexture != nullptr && ImGui::Button("Apply Active Texture"))
	{
		object->setTexture(activeTexture->name);
	}
	ImGui::Spacing();

	// Model
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Model");
	ImGui::Text("Model: %s", object->modelName.empty() ? "Cube" : object->modelName.c_str());
	Asset* activeModel = getActiveModel();
	if (activeModel != nullptr && ImGui::Button("Apply Active Model"))
	{
		object->setModel(activeModel->name);
	}
	if (!object->modelName.empty() && ImGui::Button("Reset To Cube"))
	{
		object->setModel("");
	}
	ImGui::Spacing();

	// Object Flags
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
	ImGui::Checkbox("isEnabled", &object->rigidBody3D.isEnabled);
	ImGui::Checkbox("isStatic", &object->rigidBody3D.isStatic);
	ImGui::Checkbox("isVisible", &object->display3DModel);
	ImGui::Checkbox("isDestructible", &object->isDestructible);

	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Debug");
	ImGui::Checkbox("Show Collider", &object->displayCollider);
	ImGui::Checkbox("Show Model", &object->display3DModel);
	ImGui::Checkbox("Show Direction", &object->displayDirection);
	ImGui::Separator();

	// Status
	ImGui::Text("Life Span: %f", object->lifeSpan);
	ImGui::Text("Death Span: %f", object->deathSpan);
	ImGui::Spacing();
	if (ImGui::Button("Duplicate Object"))
	{
		GameObject copy = *object;
		copy.rigidBody3D.Teleport(Vector3Add(copy.getPosition(), Vector3(1, 0, 0)));

		// saveObject may reallocate gameObjects — object is stale after this call.
		// onEnable() rebinds visuals, giving the copy its own model instead of sharing meshes
		GameObject* spawned = scene->gameMap.saveObject(copy);
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
