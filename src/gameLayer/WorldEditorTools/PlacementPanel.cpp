#include "WorldEditor.h"

// Object types a plain Game Object placement may use. Entity and Interactable
// spawn through their own placement kinds; Player is unique and never spawned here.
static constexpr ObjectType kGameObjectTypes[] = {
	OBJECT_GENERIC, OBJECT_ITEM, OBJECT_PROJECTILE, OBJECT_ENVIRONMENT
};

static const char* miniGameIdToString(int id)
{
	switch (id)
	{
	case MINI_GAME_FLAPPY_BIRD_ID: return "Flappy Bird";
	case MINI_GAME_CRANE_ID:       return "Crane";
	case MINI_GAME_DOCTOR_ID:      return "Doctor";
	case MINI_GAME_SIMON_SAYS_ID:  return "Simon Says";
	case MINI_GAME_MAZE_ID:        return "Maze";
	case MINI_GAME_RO_SHAM_BOO_ID: return "Ro Sham Boo";
	default:                       return "Unknown";
	}
}

void WorldEditor::ShowPlacementPanel()
{
	const auto manager = &SceneManager::getInstance();
	const auto scene = manager->currentScene;

	/// Show Placement Window
	ImGui::Begin("Placement");

	/** Spawn Kind **/
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "New Object");
	ImGui::RadioButton("Game Object", &placementKind, PLACE_GAME_OBJECT);
	ImGui::SameLine();
	ImGui::RadioButton("Entity", &placementKind, PLACE_ENTITY);
	ImGui::SameLine();
	ImGui::RadioButton("Interactable", &placementKind, PLACE_INTERACTABLE);
	ImGui::Separator();

	/** Base Object Data **/
	ImGui::InputText("Name: ", inputName, 128);
	{
		stagingObject.name = inputName;
	}

	if (placementKind == PLACE_GAME_OBJECT)
	{
		int typeIndex = 0;
		for (int i = 0; i < IM_ARRAYSIZE(kGameObjectTypes); ++i)
		{
			if (kGameObjectTypes[i] == stagingObject.type) { typeIndex = i; break; }
		}
		if (ImGui::Combo("Object Type", &typeIndex, "Generic\0Item\0Projectile\0Environment\0"))
		{
			stagingObject.type = kGameObjectTypes[typeIndex];
		}
	}
	else
	{
		ImGui::Text("Type: %s", placementKind == PLACE_ENTITY ? "Entity" : "Interactable");
	}

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

	/** Kind Specific Data **/
	if (placementKind == PLACE_ENTITY)
	{
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Entity");
		ImGui::InputFloat("Max Health", &stagingMaxHealth);
		ImGui::InputFloat("Max Stamina", &stagingMaxStamina);
		ImGui::InputFloat("Base Speed", &stagingBaseSpeed);
		ImGui::Separator();
	}
	else if (placementKind == PLACE_INTERACTABLE)
	{
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Interactable");
		ImGui::Combo("Interact Type", &stagingInteractType, "None\0Mini Game\0Item\0");

		if (stagingInteractType == INTERACT_MINIGAME)
		{
			if (ImGui::InputInt("Mini Game ID", &stagingInteractValue))
			{
				stagingInteractValue = Clamp(stagingInteractValue,
					MINI_GAME_FLAPPY_BIRD_ID, MINI_GAME_RO_SHAM_BOO_ID);
			}
			ImGui::Text("%s", miniGameIdToString(stagingInteractValue));
			ImGui::InputInt("Activator ID", &stagingActivatorValue);
			ImGui::TextDisabled("Object toggled when the mini game completes");
		}
		else if (stagingInteractType == INTERACT_ITEM)
		{
			ImGui::InputInt("Item ID", &stagingInteractValue);
		}
		ImGui::Separator();
	}

	// Spawn a copy so the staging object stays around for repeat placement
	auto spawnAt = [&](Vector3 position)
		{
			GameObject object = stagingObject;

			// Record assets by name — the save call binds them via onEnable()
			if (Asset* texture = getActiveTexture()) { object.textureName = texture->name; }
			if (Asset* model = getActiveModel()) { object.modelName = model->name; }

			object.rigidBody3D.translation = position;
			object.rigidBody3D.Teleport(position);

			GameObject* spawned = nullptr;
			switch (placementKind)
			{
			case PLACE_ENTITY:
			{
				Entity entity = {};
				static_cast<GameObject&>(entity) = object; // shared base fields
				entity.type = OBJECT_ENTITY; // base copy overwrote the constructor's type
				entity.maxHealth = stagingMaxHealth;
				entity.maxStamina = stagingMaxStamina;
				entity.baseSpeed = stagingBaseSpeed;
				spawned = scene->gameMap.saveEntity(entity);
				break;
			}
			case PLACE_INTERACTABLE:
			{
				InteractableObject interactable(
					static_cast<InteractionType>(stagingInteractType),
					stagingInteractValue, stagingActivatorValue);
				static_cast<GameObject&>(interactable) = object; // shared base fields
				interactable.type = OBJECT_INTERACTABLE; // base copy overwrote the constructor's type
				// saveInteractable skips the zero-scale fix saveObject/saveEntity apply
				if (interactable.rigidBody3D.scale == Vector3Zero())
				{
					interactable.rigidBody3D.scale = Vector3One();
				}
				spawned = scene->gameMap.saveInteractable(interactable);
				break;
			}
			default:
				spawned = scene->gameMap.saveObject(object);
				break;
			}

			selectedObjectId = spawned->id;
			statusMessage = "Spawned: " + spawned->name;
		};

	const char* spawnLabel =
		placementKind == PLACE_ENTITY ? "Spawn Entity" :
		placementKind == PLACE_INTERACTABLE ? "Spawn Interactable" : "Spawn Game Object";
	if (ImGui::Button(spawnLabel)) { spawnAt(stagingObject.getPosition()); }
	if (scene->player != nullptr && ImGui::Button("Spawn At Player"))
	{
		Vector3 position = Vector3Add(
			scene->player->rigidBody3D.translation,
			Vector3Scale(scene->player->camera.forward, 3.0f)
		);
		spawnAt(position);
	}
	if (scene->player != nullptr && ImGui::Button("Spawn At Camera"))
	{
		Vector3 position = Vector3Add(
			manager->camera3D.position,
			Vector3Scale(manager->camera3D.up, 3.0f)
		);
		spawnAt(position);
	}

	ImGui::End();
}
