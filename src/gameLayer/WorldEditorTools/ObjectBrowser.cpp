#include "WorldEditor.h"

void WorldEditor::showInteractableObject(InteractableObject* object) {
	ImGui::PushID(object);
	ImGui::Separator();
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Interactable");

	ImGui::Text("Interact Type: %s", interactTypeToString(object->interactType));
	int stageInteractType = static_cast<int>(object->interactType);
	if (ImGui::InputInt("InteractType", &stageInteractType)) {
		object->interactType = static_cast<InteractionType>(stageInteractType);
	}
	ImGui::InputInt("Interact Value", &object->interactValue);
	ImGui::InputInt("Activator Value", &object->activatorValue);
	ImGui::Checkbox("Interactable", &object->isInteractable);
	ImGui::Checkbox("MiniGame Running", &object->isRunningMiniGame);

	ImGui::PopID();
}

void WorldEditor::showEntity(Entity* object) {
	ImGui::PushID(object);
	ImGui::Separator();
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Entity");

	// Status
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Status");
	ImGui::Text("Health: %f", object->health);
	ImGui::Text("Max Health: %f", object->maxHealth);

	ImGui::Text("Stamina: %f", object->stamina);
	ImGui::Text("Max Stamina: %f", object->maxStamina);

	ImGui::Text("Speed: %f", object->baseSpeed);
	ImGui::Text("Max Speed: %f", object->currentSpeed);
	ImGui::Spacing();

	// Flags
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Flags");
	ImGui::Checkbox("IsSprinting", &object->isSprinting);
	ImGui::Checkbox("IsCrouching", &object->isCrouching);
	ImGui::Checkbox("IsFiring", &object->isFiring);
	ImGui::Checkbox("ForceFire", &object->forceFire);
	ImGui::Checkbox("CanAttack", &object->canAttack);
	ImGui::Spacing();

	// List Of Buffs
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "Buffs");
	for (auto& buff : object->buffTimers) {
		ImGui::PushID(&buff);
		ImGui::TextColored(ImVec4(255, 0, 255, 255), "%s", buffTypeToString(buff.cooldownID));
		ImGui::Text("Duration: %f", buff.remaining_time());
		ImGui::SameLine();
		if (ImGui::Button("Use Buff")) { buff.use(); }
		ImGui::PopID();
	}

	ImGui::PopID();
}

void WorldEditor::showGameObject(GameObject* object) {
	ImGui::PushID(object);

	std::string dataString = objectTypeToString(object->type);
	dataString += " Data";
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "%s", dataString.c_str());
	
	char nameBuffer[128] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object->name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) { object->name = nameBuffer; }

	ImGui::Text("ID: %llu", static_cast<unsigned long long>(object->id));
	ImGui::Text("Type: %d", static_cast<int>(object->type));
	ImGui::Checkbox("Selectable", &object->canBeSelected);
	ImGui::Spacing();


	// Transform (Teleport keeps the collision box in sync with the new position)
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Transform");
	Vector3 position = object->getPosition();
	if (ImGui::DragFloat3("Position: ", &position.x))
	{
		object->rigidBody3D.Teleport(position);
	}
	ImGui::SameLine();
	ImGui::Checkbox("Lock Position", &object->rigidBody3D.lockTranslation);

	// Rotation edits go through cached Euler degrees (see WorldEditor.h). Resync
	// from the quaternion only when the selection changed or something else moved
	// the rotation out from under us — angular velocity, a scene load, a script.
	const Quaternion currentRotation = object->getRotation();
	if (rotationEulerOwnerId != object->id || currentRotation != rotationEulerSource)
	{
		const Vector3 euler = QuaternionToEuler(currentRotation);
		rotationEulerDegrees = { euler.x * RAD2DEG, euler.y * RAD2DEG, euler.z * RAD2DEG };
		rotationEulerOwnerId = object->id;
		rotationEulerSource = currentRotation;
	}
	// Drag to scrub a degree at a time, ctrl+click to type an exact angle
	if (ImGui::DragFloat3("Rotation (deg): ", &rotationEulerDegrees.x, 1.0f))
	{
		const Quaternion rotation = QuaternionNormalize(QuaternionFromEuler(
			rotationEulerDegrees.x * DEG2RAD,   // pitch (X)
			rotationEulerDegrees.y * DEG2RAD,   // yaw   (Y)
			rotationEulerDegrees.z * DEG2RAD)); // roll  (Z)
		object->rigidBody3D.rotation = rotation;
		rotationEulerSource = rotation;
	}
	// Scale is applied through the model transform each frame — no mesh regen needed
	if (ImGui::DragFloat3("Scale: ", &object->rigidBody3D.scale.x))
	{
		object->rigidBody3D.scale = { object->rigidBody3D.scale.x, object->rigidBody3D.scale.y, object->rigidBody3D.scale.z };
	}
	ImGui::SameLine();
	ImGui::Checkbox("Lock Rotation", &object->rigidBody3D.lockRotation);
	ImGui::Spacing();

	// RigidBody
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "RigidBody");
	Vector3 velocity = object->rigidBody3D.GetVelocity();
	if (ImGui::InputFloat3("Velocity: ", &velocity.x))
	{
		object->rigidBody3D.velocity = velocity;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Lock Velocity", &object->rigidBody3D.lockVelocity);

	Vector3 acceleration = object->rigidBody3D.GetAcceleration();
	if (ImGui::InputFloat3("Acceleration: ", &acceleration.x))
	{
		object->rigidBody3D.acceleration = acceleration;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Lock Acceleration", &object->rigidBody3D.lockAcceleration);

	Vector3 angularVelocity = object->rigidBody3D.angularVelocity;
	if (ImGui::InputFloat3("Angular Velocity: ", &angularVelocity.x))
	{
		object->rigidBody3D.angularVelocity = angularVelocity;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Lock Angular Velocity", &object->rigidBody3D.lockAngularVelocity);
	ImGui::Spacing();

	ImGui::Text("Air Time: %f", object->rigidBody3D.GetAirTime());
	ImGui::Spacing();

	Vector4 color = Vector4(
		object->defaultColor.r,
		object->defaultColor.g,
		object->defaultColor.b,
		object->defaultColor.a);
	ImGui::TextColored(ImVec4(color.x, color.y, color.z, color.w), "Color");
	if (ImGui::InputFloat4("Color", &color.x)) {
		object->defaultColor = Color(
			color.x,
			color.y,
			color.z,
			color.w
		);
	}
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

	/*
	char* soundName;
	if (ImGui::InputText("Default Sound", soundName, 128)) {
		object->defaultSound = soundName;
	}
	char* parameterName = {};
	if (ImGui::InputText("Parameter Name", parameterName, 128)) {
		object->defaultSound = parameterName;
	}
	*/
	ImGui::InputInt("Parameter Value", &object->soundParameterValue, 1, 1);
	ImGui::Checkbox("IsLooping", &object->isLooping);// Not Implemented

	ImGui::Spacing();

	// Object Flags
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
	ImGui::Checkbox("isEnabled", &object->rigidBody3D.isEnabled);
	ImGui::Checkbox("isStatic", &object->rigidBody3D.isStatic);
	ImGui::Checkbox("isVisible", &object->display3DModel);
	ImGui::Checkbox("isDestructible", &object->isDestructible);
	ImGui::Checkbox("Can Collide", &object->rigidBody3D.canCollide);

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Debug");
	ImGui::Checkbox("Show Collider", &object->displayCollider);
	ImGui::Checkbox("Show Model", &object->display3DModel);
	ImGui::Checkbox("Show Direction", &object->displayDirection);

	// Status
	ImGui::Text("Life Span: %f", object->lifeSpan);
	ImGui::Text("Death Span: %f", object->deathSpan);
	ImGui::Spacing();
	ImGui::PopID();
}

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
		//if (getType(&object) != OBJECT_GENERIC) continue;

		ImGui::PushID(&object);
		std::string label = object.name + " (" + std::to_string(object.id) + ")";
		if (ImGui::Selectable(label.c_str(), object.id == selectedObjectId))
		{
			selectedObjectId = object.id;
		}
		ImGui::PopID();
	}

	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Entities");
	for (auto& object : scene->entities)
	{
		ImGui::PushID(&object);
		std::string label = object.second.get()->name + " (" + std::to_string(object.second.get()->id) + ")";
		if (ImGui::Selectable(label.c_str(), object.second.get()->id == selectedObjectId))
		{
			selectedObjectId = object.second.get()->id;
		}
		ImGui::PopID();
	}

	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Interactables");
	for (auto& object : scene->interactables)
	{
		ImGui::PushID(&object);
		std::string label = object.second.get()->name + " (" + std::to_string(object.second.get()->id) + ")";
		if (ImGui::Selectable(label.c_str(), object.second.get()->id == selectedObjectId))
		{
			selectedObjectId = object.second.get()->id;
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

	// Show Object Data
	
	// Push ID in GameObject
	showGameObject(object);
	if (object->type == OBJECT_ENTITY) {
		showEntity(scene->entities[object->id].get());
	}
	else if (object->type == OBJECT_INTERACTABLE) {
		showInteractableObject(scene->interactables[object->id].get());
	}

	// Both route through the shared commands so the buttons, the keyboard
	// shortcuts and the viewport all take the identical path — including
	// cancelling any in-flight gizmo drag before the object is destroyed, and
	// recording the spawn so Ctrl+Z can take it back.
	if (ImGui::Button("Duplicate Object")) { DuplicateSelection(); }
	ImGui::SameLine();
	if (ImGui::Button("Delete Object")) { DeleteSelection(); }

	ImGui::EndChild();
	ImGui::End();
}
