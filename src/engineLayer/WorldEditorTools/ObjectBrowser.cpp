#include "WorldEditor.h"

#include <concepts>

#pragma region Apply All State Buttons

static void StaticButtons(Scene* scene) {
	/// Static/NonStatic All Objects
	if (ImGui::Button("Make All Static")) {
		std::cout << "[Object Browser] Set All Objects Static\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.rigidBody3D.isStatic = true;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->rigidBody3D.isStatic = true;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->rigidBody3D.isStatic = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Make All Not Static")) {
		std::cout << "[Object Browser] Set All Objects NonStatic\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.rigidBody3D.isStatic = false;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->rigidBody3D.isStatic = false;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->rigidBody3D.isStatic = false;
		}

	}
	ImGui::Spacing();
}

static void DisplayDirectionButtons(Scene* scene) {
	/// Display Direction All Objects
	if (ImGui::Button("Show Direction")) {
		std::cout << "[Object Browser] Show All Objects Direction\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.displayDirection = true;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->displayDirection = true;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->displayDirection = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Hide Direction")) {
		std::cout << "[Object Browser] Hide All Objects Direction\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.displayDirection = false;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->displayDirection = false;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->displayDirection = false;
		}
	}
	ImGui::Spacing();
}

static void IsSelectableButtons(Scene* scene) {
	/// IsSelectable All Objects
	if (ImGui::Button("Make All Selectable")) {
		std::cout << "[Object Browser] Set All Objects Selectable\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.isSelectable = true;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->isSelectable = true;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->isSelectable = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Make All Not Selectable")) {
		std::cout << "[Object Browser] Set All Objects Not Selectable\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.isSelectable = false;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->isSelectable = false;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->isSelectable = false;
		}
	}
	ImGui::Spacing();
}

static void EnableButtons(Scene* scene) {
	/// Enable/Disable All Objects
	if (ImGui::Button("Make All Enabled")) {
		std::cout << "[Object Browser] Set All Objects Enabled\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.isEnabled = true;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->isEnabled = true;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->isEnabled = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Make All Not Enabled")) {
		std::cout << "[Object Browser] Set All Objects Not Enabled\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.isEnabled = false;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->isEnabled = false;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->isEnabled = false;
		}
	}
	ImGui::Spacing();
}
static void DestructableButton(Scene* scene) {
	/// Destructable All Objects
	if (ImGui::Button("Make All Destructable")) {
		std::cout << "[Object Browser] Set All Objects Destructable\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.isDestructible = true;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->isDestructible = true;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->isDestructible = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Make All Not Destructable")) {
		std::cout << "[Object Browser] Set All Objects Not Destructable\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.isDestructible = false;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->isDestructible = false;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->isDestructible = false;
		}
	}
	ImGui::Spacing();
}


static void DisplayColliderButtons(Scene* scene) {
	/// Enable/Disable All Objects
	if (ImGui::Button("Show Collider")) {
		std::cout << "[Object Browser] Show All Colliders\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.displayCollider = true;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->displayCollider = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Hide Collider")) {
		std::cout << "[Object Browser] Show All Colliders\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.displayCollider = false;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->displayCollider = false;
		}

	}
	ImGui::Spacing();
}

#pragma endregion

#pragma region Apply All State Buttons (RigidBody3D)

static void CanCollideButtons(Scene* scene) {
	/// Enable/Disable All Objects
	if (ImGui::Button("Make All Can Collide")) {
		std::cout << "[Object Browser] Set All Objects Can Collide\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.rigidBody3D.canCollide = true;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->rigidBody3D.canCollide = true;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->rigidBody3D.canCollide = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Make All Not Can Collide")) {
		std::cout << "[Object Browser] Set All Objects Not Can Collide\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.rigidBody3D.canCollide = false;
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->rigidBody3D.canCollide = false;
		}
		for (auto& [id, object] : scene->gameMap.entities) {
			object->rigidBody3D.canCollide = false;
		}
	}
	ImGui::Spacing();
}

static void FitModelToColliderButtons(Scene* scene) {
	/// Enable/Disable All Objects
	if (ImGui::Button("Fit Model To Collider")) {
		std::cout << "[Object Browser] Fit Model To Collider\n";
		for (auto& object : scene->gameMap.gameObjects) {
			object.rigidBody3D.collider.FitToModel(object.model);
		}
		for (auto& [id, object] : scene->gameMap.interactables) {
			object->rigidBody3D.collider.FitToModel(object->model);
		}
	}
	ImGui::Spacing();
}

#pragma endregion


void WorldEditor::showInteractableObject(InteractableObject* object) {
	ImGui::PushID(object);
	ImGui::Separator();
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Interactable");
	ImGui::Checkbox("Interactable", &object->isInteractable);

	ImGui::Text("Interact Type: %s", interactTypeToString(object->interactType));
	int stageInteractType = static_cast<int>(object->interactType);
	if (ImGui::InputInt("InteractType", &stageInteractType)) {
		object->interactType = static_cast<InteractionType>(stageInteractType);
	}

	// MiniGame Variables 
	if (object->interactType == INTERACT_MINIGAME) {
		ImGui::InputInt("Variation", &object->variation);
		ImGui::TextDisabled("%s", miniGameIdToString(object->variation));
		ImGui::Checkbox("MiniGame Running", &object->isRunningMiniGame);
		ImGui::Checkbox("Completed", &object->isCompleted);
		// Worth stating outright: the Director stops hinting toward a station the
		// moment this is ticked, so it doubles as the switch for testing whether
		// hinting moves on to the next outstanding objective.
		ImGui::TextDisabled("Completed stations are skipped by the Director");
	}

	if (object->interactType == INTERACT_MINIGAME || object->interactType == INTERACT_ITEM) {
		ImGui::InputInt("Activator Value", &object->activator);

		if (object->interactType == INTERACT_MINIGAME) {
			// activator == 0 (or an id nothing resolves to) means "no explicit
			// target" — FindWorldObjectByID returns nullptr for that, and
			// dereferencing it here crashed the editor on the common
			// self-activating case (see PlacementPanel's "Activator ID" field).
			if (GameObject* activatorObject = FindWorldObjectByID(SceneManager::getInstance().currentScene->gameMap, object->activator)) {
				ImGui::TextDisabled("%s", activatorObject->name.c_str());
			}
			else {
				ImGui::TextDisabled("None (self)");
			}
		}
		if (ImGui::Button("Self Activator")) { object->activator = object->id; }
	}

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
	ImGui::Checkbox("Selectable", &object->isSelectable);
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
	ImGui::SameLine();
	ImGui::Checkbox("Lock Rotation", &object->rigidBody3D.lockRotation);
	
	// Scale is applied through the model transform each frame — no mesh regen needed
	if (ImGui::DragFloat3("Scale: ", &object->rigidBody3D.scale.x))
	{
		// Every other scale path clamps (the gizmo, ApplyTransform, SpawnStagedObject).
		// Dragging this field negative inverts the collision box, and per
		// EditorPicking.h an inverted box is neither pickable nor collidable — the
		// object would become impossible to re-select in the viewport to undo it.
		object->rigidBody3D.scale = SanitizeScale(object->rigidBody3D.scale);
	}
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

	// Collider — the collision volume, which is deliberately not the render scale
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Collider");
	Collider3D& collider = object->rigidBody3D.collider;

	// Label arrays sized by the enum's own _COUNT, following LightingInspector,
	// so adding a shape later cannot silently desync the labels from the values.
	const char* shapeLabels[COLLIDER_SHAPE_COUNT] = {};
	for (int i = 0; i < COLLIDER_SHAPE_COUNT; ++i) { shapeLabels[i] = colliderShapeToString(i); }
	const char* modeLabels[COLLIDER_MODE_COUNT] = {};
	for (int i = 0; i < COLLIDER_MODE_COUNT; ++i) { modeLabels[i] = colliderModeToString(i); }

	int shapeIndex = static_cast<int>(collider.shape);
	if (ImGui::Combo("Shape", &shapeIndex, shapeLabels, COLLIDER_SHAPE_COUNT))
	{
		collider.shape = static_cast<ColliderShape>(shapeIndex);
		// Switching to Mesh should show the fitted volume straight away rather than
		// leaving the last hand-authored size sitting on screen looking authoritative
		if (collider.shape == COLLIDER_MESH) { collider.FitToModel(object->model); }
	}

	int modeIndex = static_cast<int>(collider.mode);
	if (ImGui::Combo("Mode", &modeIndex, modeLabels, COLLIDER_MODE_COUNT))
	{
		collider.mode = static_cast<ColliderMode>(modeIndex);
	}
	ImGui::TextDisabled("%s", collider.isTrigger()
		? "Reports overlaps, applies no physics."
		: "Resolves overlaps through the rigid body.");

	if (collider.shape == COLLIDER_SPHERE)
	{
		if (ImGui::DragFloat("Radius", &collider.radius, 0.01f))
		{
			collider.radius = fmaxf(collider.radius, MINIMUM_COLLIDER_EXTENT);
		}
	}
	else if (collider.shape == COLLIDER_MESH)
	{
		// Derived from the model's own bounds, so presenting it as editable would
		// be a lie — the next loadVisuals() would overwrite anything typed here
		ImGui::BeginDisabled();
		ImGui::DragFloat3("Size (fitted)", &collider.size.x);
		ImGui::EndDisabled();
	}
	else
	{
		if (ImGui::DragFloat3("Size", &collider.size.x, 0.01f))
		{
			// A zero or negative extent inverts the volume, and per EditorPicking.h
			// an inverted box is neither collidable nor clickable — the object goes
			// silently inert with nothing on screen to explain why.
			collider.size = SanitizeColliderSize(collider.size);
		}
	}

	ImGui::DragFloat3("Offset", &collider.offset.x, 0.01f);

	if (ImGui::Button("Fit To Model")) { collider.FitToModel(object->model); }
	ImGui::SameLine();
	if (ImGui::Button("Reset Collider"))
	{
		// Back to the volume the body had before colliders existed, keeping the
		// mode — resetting the shape is the common need, changing solid to trigger
		// by accident is not.
		const ColliderMode keptMode = collider.mode;
		collider = {};
		collider.mode = keptMode;
	}

	// The World Editor freezes the simulation, so nothing else is refreshing the
	// broad-phase box while these fields are being dragged — and mouse picking
	// and the debug draw both read it.
	object->rigidBody3D.SyncBroadPhaseBox();
	ImGui::Spacing();

	Vector4 color = Vector4(
		object->defaultColor.r,
		object->defaultColor.g,
		object->defaultColor.b,
		object->defaultColor.a);
	// ImVec4 color channels are normalized 0-1, unlike the 0-255 Color/Vector4 above.
	ImGui::TextColored(ImVec4(color.x / 255.0f, color.y / 255.0f, color.z / 255.0f, color.w / 255.0f), "Color");
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
	// A model whose load failed can carry materialCount == 0 with a null
	// materials array (see LightingSystem::ApplyToModel's same guard) —
	// indexing materials[0] unconditionally would crash the inspector on it.
	if (object->model.materialCount > 0 && object->model.materials != nullptr)
	{
		ImGui::Image((ImTextureRef)(intptr_t)object->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id, ImVec2(64, 64));
	}
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
	ImGui::Checkbox("Show Direction", &object->displayDirection);

	// Status
	ImGui::Text("Life Span: %f", object->lifeSpan);
	ImGui::Text("Death Span: %f", object->deathSpan);
	ImGui::Spacing();
	ImGui::PopID();
}

static void ShowStateButtons(Scene* scene) {

	ImGui::BeginChild("State Buttons", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.3f));
	
	EnableButtons(scene);
	StaticButtons(scene);
	DisplayColliderButtons(scene);
	DisplayDirectionButtons(scene);
	IsSelectableButtons(scene);
	DestructableButton(scene);
	CanCollideButtons(scene);
	FitModelToColliderButtons(scene);
	
	ImGui::EndChild();
	
	ImGui::Separator();
}

static void ObjectList(std::string displayName, std::vector<GameObject>& objects, bool& show, uint64_t& selectID) {
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "%s", displayName.c_str());
	ImGui::SameLine();
	std::string buttonLabel = show ? "Hide" + displayName : "Show" + displayName;

	if (ImGui::ArrowButton(buttonLabel.c_str(), show ? ImGuiDir_Down : ImGuiDir_Left)) { show = !show; }
	if (show) {

		for (const auto& object : objects)
		{
			ImGui::PushID(&object);
			std::string label = object.name + " (" + std::to_string(object.id) + ")";
			if (ImGui::Selectable(label.c_str(), object.id == selectID))
			{
				selectID = object.id;
			}
			ImGui::PopID();
		}
	}
}
static void ObjectList(std::string displayName, std::unordered_map<uint64_t, std::unique_ptr<Entity>>& objects, bool& show, uint64_t& selectID) {
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "%s", displayName.c_str());
	ImGui::SameLine();
	std::string buttonLabel = show ? "Hide" + displayName : "Show" + displayName;

	if (ImGui::ArrowButton(buttonLabel.c_str(), show ? ImGuiDir_Down : ImGuiDir_Left)) { show = !show; }
	if (show) {

		for (const auto& [id, object] : objects)
		{
			ImGui::PushID(&object);
			std::string label = object->name + " (" + std::to_string(object->id) + ")";
			if (ImGui::Selectable(label.c_str(), object->id == selectID))
			{
				selectID = object->id;
			}
			ImGui::PopID();
		}
	}
}
static void ObjectList(std::string displayName, std::unordered_map<uint64_t, std::unique_ptr<InteractableObject>>& objects, bool& show, uint64_t& selectID) {
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "%s", displayName.c_str());
	ImGui::SameLine();
	std::string buttonLabel = show ? "Hide" + displayName : "Show" + displayName;

	if (ImGui::ArrowButton(buttonLabel.c_str(), show ? ImGuiDir_Down : ImGuiDir_Left)) { show = !show; }
	if (show) {

		for (const auto& [id, object] : objects)
		{
			ImGui::PushID(&object);
			std::string label = object->name + " (" + std::to_string(object->id) + ")";
			if (ImGui::Selectable(label.c_str(), object->id == selectID))
			{
				selectID = object->id;
			}
			ImGui::PopID();
		}
	}
}

static void ShowObjectList(Scene* scene, uint64_t& selectID, bool& objects, bool& entities, bool& interactable) {

	ImGui::BeginChild("World Object List", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.3f));

	ObjectList("Game Objects", scene->gameMap.gameObjects, objects, selectID);
	ObjectList("Entities", scene->gameMap.entities, entities, selectID);
	ObjectList("Interactables", scene->gameMap.interactables, interactable, selectID);

	ImGui::EndChild();
	ImGui::Separator();
}

void WorldEditor::showSelectedObject(Scene* scene) {
	ImGui::BeginChild("Selected Object");

	GameObject* object = getSelectedObject();
	if (object == nullptr)
	{
		ImGui::Text("Nothing selected");
		ImGui::EndChild();
		return;
	}

	// Show Object Data

	// Push ID in GameObject
	showGameObject(object);
	// ObjectType comes straight out of the save file and does not imply map
	// membership — the selected object was found in gameMap.gameObjects. operator[]
	// on a miss would both return a null unique_ptr to dereference here and leave a
	// null entry behind for Scene_updateScene to trip over every frame after.
	if (object->type == OBJECT_ENTITY && scene->gameMap.entities.contains(object->id)) {
		showEntity(scene->gameMap.entities[object->id].get());
	}
	else if (object->type == OBJECT_INTERACTABLE && scene->gameMap.interactables.contains(object->id)) {
		showInteractableObject(scene->gameMap.interactables[object->id].get());
	}

	// Both route through the shared commands so the buttons, the keyboard
	// shortcuts and the viewport all take the identical path — including
	// cancelling any in-flight gizmo drag before the object is destroyed, and
	// recording the spawn so Ctrl+Z can take it back.
	if (ImGui::Button("Duplicate Object")) { DuplicateSelection(); }
	ImGui::SameLine();
	if (ImGui::Button("Delete Object")) { DeleteSelection(); }

	ImGui::EndChild();
}

void WorldEditor::ShowObjectBrowser()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show Object Browser Window
	ImGui::Begin("Object Browser");

	ShowStateButtons(scene);
	
	ShowObjectList(scene, selectedObjectId, showGameObjects, showEntities, showInteractables);

	showSelectedObject(scene);

	ImGui::End();
}
