#include <GameObject.h>

#include <SceneManager.h>
#include <gameMap.h>


void ActivateMiniGame(SceneManagement::Scene& scene, InteractableObject* interactable, bool bypass = false)
{
	std::cout << "[InteractableObject] Activating MiniGame: " << interactable->variation << "\n";

	scene.soundField.Emit(interactable->getPosition(), 0.8f, SOUND_TAMPER, interactable->id);

	if (!bypass && scene.player->artifactUnlocked < interactable->variation) { std::cout << "[InteractableObject] Minigame Not Unlocked \n"; return; }

	// The scene marks the station as occupied, and on a win finds the Target
	// through the station itself
	scene.StartMiniGame(interactable->variation, interactable);
}

GameObject* InteractableObject::FindMiniGameTarget(GameMap& gameMap)
{
	if (targetId == 0) { return this; }
	return gameMap.FindWorldObject(targetId);
}

void Unlock(SceneManagement::Scene& scene, InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Unlocking MiniGame: " << interactable->variation << "\n";
	auto player = scene.player;
	player->artifactUnlocked = std::max(player->artifactUnlocked, interactable->variation);
	player->artifactUnlocked = Clamp(player->artifactUnlocked, MINI_GAME_FLAPPY_BIRD_ID, MINI_GAME_RO_SHAM_BOO_ID);

	// Check If Player Has Artifact
	if (player->artifact == nullptr) {
		std::cout << "Create Artifact \n";
		// Create Artifact
		player->artifact = new GameObject;
		player->artifact->name = "Artifact";
		player->artifact->isDestructible = false;
		player->artifact->rigidBody3D.scale = Vector3One();
		player->artifact->rigidBody3D.collider.canCollide = false;
		player->artifact->rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f);
		player->artifact->rigidBody3D.angularVelocity = Vector3(0.0f, 1.0f, 0.0f);
		player->artifact->rigidBody3D.lockAngularVelocity = true;
		player->artifact->defaultSound = "Artifact_Load_Up";

		// Set Model Based On Upgrade
		std::string model = "RubixCube" + std::to_string(player->artifactUnlocked);
		player->artifact->setModel(model);
	}
	else {
		// Set Model Based On Upgrade
		std::string model = "RubixCube" + std::to_string(player->artifactUnlocked);
		player->artifact->setModel(model);
	}

	interactable->isInteractable = false;

	// Removed at the end of the frame by GameMap::SweepDestroyed, not here: this
	// runs inside the station's own onInteract
	interactable->Destroy();
}

void AddItemToInventory(SceneManagement::Scene& scene, InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Adding Item to Inventory: " << interactable->activator << "\n";

	// Search in vector
	auto it = std::ranges::find(scene.player->inventory, interactable->id);

	if (it != scene.player->inventory.end()) {
		// Item already in inventory
		return;
	}

	scene.player->inventory.push_back(interactable->id);
	interactable->onDisable();

	std::cout << "[InteractableObject] Item added to inventory.\n";
}

InteractableObject::InteractableObject(const InteractionType interact, int value)
{
	type = OBJECT_INTERACTABLE;
	interactType = interact; // Type Of Interaction
	activator = value; // Item
}
InteractableObject::InteractableObject(const InteractionType interact, int variant, int activate )
{
	type = OBJECT_INTERACTABLE;
	interactType = interact; // Type of interaction
	variation = variant; // Minigame
	activator = activate; // Object
}

void InteractableObject::onInteract(SceneManagement::Scene& scene)
{
	AudioManager::AudioManager::getInstance().Play3D(defaultSound, *this);
	std::cout << "[InteractObject] Interacted with " << name << "\n";

	// Play3D only populates soundInstance when it falls back to the FMOD Studio event path;
	// a plain loaded sound (the common case) leaves it null.
	if (soundInstance != nullptr) {
		soundInstance->setParameterByName(soundParameterName.c_str(), soundParameterValue);
	}

	// The branches stay mutually exclusive: Unlock() marks this station for
	// removal, and none of the others may run on a station that is going away.
	const InteractionType type = interactType;
	if (type == INTERACT_MINIGAME) { ActivateMiniGame(scene, this); }
	else if (type == INTERACT_UNLOCK) { Unlock(scene, this); }
	else if (type == INTERACT_ITEM) { AddItemToInventory(scene, this); }

}

Json InteractableObject::formatToJson()
{
	Json j;
	addCommonToJson(j);
	j["InteractType"] = interactType;
	j["Variation"] = variation;
	j["Activator"] = activator;
	j["TargetId"] = targetId;
	j["IsInteractable"] = isInteractable;
	j["IsCompleted"] = isCompleted;
	// isRunningMiniGame is deliberately not persisted: it means "occupied right
	// now", and a save reloaded later has nobody standing at the station.
	return j;
}

bool InteractableObject::loadFromJson(Json& j)
{
	if (!loadCommonFromJson(j)) { return false; }
	interactType = static_cast<InteractionType>(j.value("InteractType", 0));
	variation = j.value("Variation", 0);
	// 0 is the "no activator" sentinel every other path uses — both constructors
	// default to it and ActivateMiniGame tests against it. Defaulting to -1 made
	// a save written before ActivatorValue existed load as a real-but-
	// unresolvable id instead of "none".
	activator = j.value("Activator", 0);

	// Saves written before a station had a Target of its own kept it in Activator
	if (j.contains("TargetId")) {
		targetId = j.value("TargetId", std::uint64_t{ 0 });
	}
	else if (interactType == INTERACT_MINIGAME) {
		targetId = static_cast<std::uint64_t>(std::max(activator, 0));
		activator = 0;
	}

	isInteractable = j.value("IsInteractable", true);
	isCompleted = j.value("IsCompleted", false);

	// A station is never occupied on load, whatever was happening when the save
	// was written — the same reason the Stalker drops its lastKnownPosition.
	isRunningMiniGame = false;
	return true;
}
