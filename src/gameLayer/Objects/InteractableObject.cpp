#include <GameObject.h>

#include <SceneManager.h>
#include <gameMap.h>


void ActivateMiniGame(InteractableObject* interactable, bool bypass = false)
{
	std::cout << "[InteractableObject] Activating MiniGame: " << interactable->variation << "\n";
	auto scene = SceneManager::getInstance().currentScene;

	// Tampering (plan's emitter table: ~0.8, one-shot): the clunk of the player
	// physically working a station, as distinct from the hum the station then
	// makes while it runs. At the stalker's default thresholds 0.8 clears
	// huntThreshold out to roughly nine unoccluded metres and stays above
	// hearingThreshold to about thirty, so starting a task is a commitment
	// anywhere in a one-room slice rather than a free action.
	//
	// Emitted ahead of the unlock gate on purpose: rattling a station you cannot
	// open is still interfering with it, and a failed attempt that costs nothing
	// would be the safest way to play. Attributed to the station, so the noise
	// carries where the machine is rather than who touched it.
	//
	// The dedicated tamper mechanic belongs to the task-station ticket; when it
	// lands it should emit here too rather than inventing a second channel.
	scene->soundField.Emit(interactable->getPosition(), 0.8f, SOUND_TAMPER, interactable->id);

	if (!bypass && scene->player->artifactUnlocked < interactable->variation) { std::cout << "[InteractableObject] Minigame Not Unlocked \n"; return; }

	scene->SetMiniGame(interactable->variation);
	// activator == 0 means "no explicit target" — the minigame should toggle
	// the interactable that launched it rather than nothing at all.
	scene->player->interactObjectId = interactable->activator != 0
		? interactable->activator
		: interactable->id;
	interactable->isRunningMiniGame = true;
}

void Unlock(InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Unlocking MiniGame: " << interactable->variation << "\n";
	auto player = SceneManager::getInstance().currentScene->player;
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
		player->artifact->rigidBody3D.canCollide = false;
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
	interactable->Destroy();
}

void AddItemToInventory(InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Adding Item to Inventory: " << interactable->activator << "\n";
	auto scene = SceneManager::getInstance().currentScene;

	// Search in vector
	auto it = std::ranges::find(scene->player->inventory, interactable);

	if (it != scene->player->inventory.end()) {
		// Item already in inventory
		return;
	}
	
	scene->player->inventory.push_back(interactable);
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

void InteractableObject::onInteract()
{
	AudioManager::getInstance().Play3D(defaultSound, *this);
	std::cout << "[InteractObject] Interacted with " << name << "\n";

	// Play3D only populates soundInstance when it falls back to the FMOD Studio event path;
	// a plain loaded sound (the common case) leaves it null.
	if (soundInstance != nullptr) {
		soundInstance->setParameterByName(soundParameterName.c_str(), soundParameterValue);
	}

	if (interactType == INTERACT_MINIGAME) { ActivateMiniGame(this); }
	if (interactType == INTERACT_UNLOCK) { Unlock(this); }
	if (interactType == INTERACT_ITEM) { AddItemToInventory(this); }

}

Json InteractableObject::formatToJson()
{
	Json j;
	addCommonToJson(j);
	j["InteractType"] = interactType;
	j["Variation"] = variation;
	j["Activator"] = activator;
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
	isInteractable = j.value("IsInteractable", true);
	isCompleted = j.value("IsCompleted", false);

	// A station is never occupied on load, whatever was happening when the save
	// was written — the same reason the Stalker drops its lastKnownPosition.
	isRunningMiniGame = false;
	return true;
}
