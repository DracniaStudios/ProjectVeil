#include "WorldEditor.h"

// Object types a plain Game Object placement may use. Entity and Interactable
// spawn through their own placement kinds; Player is unique and never spawned here.
static constexpr ObjectType kGameObjectTypes[] = {
	OBJECT_GENERIC, OBJECT_ITEM, OBJECT_PROJECTILE, OBJECT_ENVIRONMENT
};

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
	ImGui::Checkbox("Lock Position", &stagingObject.rigidBody3D.lockTranslation);
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
	ImGui::Spacing();

	// Collider — staged here so a trigger volume can be dropped in directly
	// rather than placed solid and then converted in the Object Browser.
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Collider");
	const char* shapeLabels[COLLIDER_SHAPE_COUNT] = {};
	for (int i = 0; i < COLLIDER_SHAPE_COUNT; ++i) { shapeLabels[i] = colliderShapeToString(i); }
	const char* modeLabels[COLLIDER_MODE_COUNT] = {};
	for (int i = 0; i < COLLIDER_MODE_COUNT; ++i) { modeLabels[i] = colliderModeToString(i); }

	int stagingShape = static_cast<int>(stagingObject.rigidBody3D.collider.shape);
	if (ImGui::Combo("Collider Shape", &stagingShape, shapeLabels, COLLIDER_SHAPE_COUNT))
	{
		stagingObject.rigidBody3D.collider.shape = static_cast<ColliderShape>(stagingShape);
	}
	int stagingMode = static_cast<int>(stagingObject.rigidBody3D.collider.mode);
	if (ImGui::Combo("Collider Mode", &stagingMode, modeLabels, COLLIDER_MODE_COUNT))
	{
		stagingObject.rigidBody3D.collider.mode = static_cast<ColliderMode>(stagingMode);
	}
	// Size and offset are left to the Object Browser: they are almost always
	// tuned against the placed object with Show Collider on, not guessed up front.
	ImGui::Separator();

	/** Kind Specific Data **/
	if (placementKind == PLACE_ENTITY)
	{
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Entity");
		// Order matches the EntityKind enum. Player is offered because the enum
		// carries it, but spawning one here is not meaningful — the scene owns
		// exactly one player, constructed in Scene_new.
		ImGui::Combo("Entity Kind", &stagingEntityKind, "Entity\0Player\0Stalker\0");
		if (stagingEntityKind == ENTITYKIND_PLAYER)
		{
			ImGui::TextColored(ImVec4(255, 255, 0, 255),
				"Scene::player is the only player; this spawns an inert duplicate.");
		}
		ImGui::InputFloat("Max Health", &stagingMaxHealth);
		ImGui::InputFloat("Max Stamina", &stagingMaxStamina);
		ImGui::InputFloat("Base Speed", &stagingBaseSpeed);
		ImGui::Separator();
	}
	else if (placementKind == PLACE_INTERACTABLE)
	{
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Interactable");
		ImGui::Combo("Interact Type", &stagingInteractType, "None\0Mini Game\0Unlock\0Item\0");

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
		else if (stagingInteractType == INTERACT_UNLOCK) {
			// Feeds player->artifactUnlocked, not an inventory item.
			ImGui::InputInt("Artifact Tier", &stagingInteractValue);
		}
		else if (stagingInteractType == INTERACT_ITEM)
		{
			ImGui::InputInt("Item ID", &stagingInteractValue);
		}
		ImGui::Separator();
	}

	/** Point & Place **/
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Point & Place");
	ImGui::Checkbox("Armed (P)", &placementMode);
	if (placementMode)
	{
		ImGui::TextDisabled("Left click in the viewport to place, Esc to cancel");
	}
	ImGui::Checkbox("Rest On Surface", &placeAlignToSurface);
	ImGui::Checkbox("Snap To Grid", &placementSnap);
	if (ImGui::DragFloat("Grid Step", &placementGridStep, 0.05f, 0.0f, 100.0f))
	{
		// A zero or negative step would divide by zero inside SnapValue; it is
		// treated as "no snapping" there, but clamping keeps the field honest.
		placementGridStep = fmaxf(placementGridStep, 0.0f);
	}
	ImGui::Separator();

	const char* spawnLabel =
		placementKind == PLACE_ENTITY ? "Spawn Entity" :
		placementKind == PLACE_INTERACTABLE ? "Spawn Interactable" : "Spawn Game Object";
	if (ImGui::Button(spawnLabel)) { SpawnStagedObject(stagingObject.getPosition()); }
	if (scene->player != nullptr && ImGui::Button("Spawn At Player"))
	{
		Vector3 position = Vector3Add(
			scene->player->rigidBody3D.translation,
			Vector3Scale(scene->player->camera.forward, 3.0f)
		);
		SpawnStagedObject(position);
	}
	if (ImGui::Button("Spawn At Camera"))
	{
		Vector3 position = Vector3Add(
			manager->camera3D.position,
			Vector3Scale(manager->camera3D.up, 3.0f)
		);
		SpawnStagedObject(position);
	}

	ImGui::End();
}

/**
 * Spawns a copy of the staging object at a world position.
 *
 * Shared by the buttons above and by point-and-place, so the two paths cannot
 * drift apart — the subtle part is that each placement kind has to be built
 * through its own type. Assigning through the GameObject base would slice off an
 * Entity's or Interactable's own fields while leaving `type` claiming otherwise,
 * and a later lookup by that type would then index a container the object was
 * never inserted into.
 *
 * The staging object itself is never handed to the scene, so the panel keeps its
 * settings for the next placement.
 */
GameObject* WorldEditor::SpawnStagedObject(Vector3 position)
{
	auto scene = SceneManager::getInstance().currentScene;
	if (scene == nullptr) { statusMessage = "No scene loaded"; return nullptr; }

	GameObject object = stagingObject;

	// The copy inherited the staging object's model handle *and* its ownsModel
	// flag, but staging is still using that model — and will keep using it for
	// the next placement. Without disowning, the loadVisuals() inside the save
	// call below would free it out from under the panel.
	object.disownModel();

	// Record assets by name — the save call binds them via onEnable()
	if (Asset* texture = getActiveTexture()) { object.textureName = texture->name; }
	if (Asset* model = getActiveModel()) { object.modelName = model->name; }

	object.rigidBody3D.Teleport(position);

	// saveObject/saveEntity only repair an exactly-zero scale. A negative extent
	// typed into the panel would survive into the world and invert the object's
	// collision box, leaving it both unclickable and uncollidable.
	if (object.rigidBody3D.scale != Vector3Zero())
	{
		object.rigidBody3D.scale = SanitizeScale(object.rigidBody3D.scale);
	}

	GameObject* spawned = nullptr;
	switch (placementKind)
	{
	case PLACE_ENTITY:
	{
		// Built through the factory so the spawned object really is the
		// requested subclass rather than a base Entity wearing its name.
		// saveEntity() stores it via the virtual clone(), so a Stalker survives
		// the copy into Scene::entities intact.
		auto entity = Entity::createByKind(static_cast<EntityKind>(stagingEntityKind));

		// Defaults the subclass constructor set that live on GameObject, and so
		// would be flattened by the base assignment below. baseDamage is the one
		// that matters: the placement panel has no field for it, so without this
		// a spawned Stalker inherits the staging object's damage and lands on
		// the player for whatever a scenery cube happens to carry.
		const float kindDamage = entity->baseDamage;
		const std::string kindName = entity->name;

		// The Entity constructor runs GameObject's, which generates a fallback
		// cube. The assignment below overwrites that handle wholesale, so
		// without this the mesh is orphaned — one leak per Entity spawned.
		// Safe to free unconditionally: this object was constructed on the line
		// above and nothing has copied it.
		entity->releaseGeneratedModel();
		static_cast<GameObject&>(*entity) = object; // shared base fields
		entity->type = OBJECT_ENTITY; // base copy overwrote the constructor's type
		entity->maxHealth = stagingMaxHealth;
		entity->maxStamina = stagingMaxStamina;
		entity->baseSpeed = stagingBaseSpeed;

		if (stagingEntityKind != ENTITYKIND_NONE)
		{
			entity->baseDamage = kindDamage;
			// Only reclaim the name when the staging object was never renamed,
			// so an explicit name from the panel still wins.
			if (entity->name == "GameObject") { entity->name = kindName; }
		}

		spawned = scene->gameMap.saveEntity(*entity);
		break;
	}
	case PLACE_INTERACTABLE:
	{
		InteractableObject interactable(
			static_cast<InteractionType>(stagingInteractType),
			stagingInteractValue, stagingActivatorValue);
		// Same as the Entity branch: release the constructor's fallback cube
		// before the assignment orphans it.
		interactable.releaseGeneratedModel();
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

	if (spawned == nullptr) { statusMessage = "Spawn failed"; return nullptr; }

	// saveObject/saveEntity repair a zero scale, but none of them rebuild the
	// collision box afterwards — and with the simulation frozen nothing else
	// will. A freshly placed object would be invisible to the very mouse ray
	// that placed it until physics was resumed.
	spawned->rigidBody3D.SyncCollisionBox();

	selectedObjectId = spawned->id;
	statusMessage = "Spawned: " + spawned->name;
	return spawned;
}
