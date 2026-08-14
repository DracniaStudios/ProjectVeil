#include <GameObject.h>

#include <SceneManager.h>
#include <gameMap.h>


void ActivateMiniGame(InteractableObject* interactable, bool bypass = false)
{
	std::cout << "[InteractableObject] Activating MiniGame: " << interactable->interactValue << "\n";
	auto scene = SceneManager::getInstance().currentScene;

	if (!bypass && scene->player->artifactUnlocked < interactable->interactValue) { std::cout << "[InteractableObject] Minigame Not Unlocked \n"; return; }
		
	scene->SetMiniGame(interactable->interactValue);
	scene->player->interactObject = interactable->activatorValue != 0 ? scene->gameMap.FindGameObjectByID(interactable->activatorValue) : interactable;
	interactable->isRunningMiniGame = true;
}

void Unlock(InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Unlocking MiniGame: " << interactable->interactValue << "\n";
	auto player = SceneManager::getInstance().currentScene->player;
	player->artifactUnlocked = std::max(player->artifactUnlocked, interactable->interactValue);

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
}

void AddItemToInventory(InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Adding Item to Inventory: " << interactable->interactValue << "\n";
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
	interactValue = value; // Item
}
InteractableObject::InteractableObject(const InteractionType interact, int value, int activator )
{
	type = OBJECT_INTERACTABLE;
	interactType = interact; // Type of interaction
	interactValue = value; // Minigame
	activatorValue = activator; // Object
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
	j["InteractValue"] = interactValue;
	j["ActivatorValue"] = activatorValue;
	j["IsInteractable"] = isInteractable;
	// Maybe Is Completed
	return j;
}

bool InteractableObject::loadFromJson(Json& j)
{
	if (!loadCommonFromJson(j)) { return false; }
	interactType = static_cast<InteractionType>(j.value("InteractType", 0));
	interactValue = j.value("InteractValue", 0);
	activatorValue = j.value("ActivatorValue", -1);
	isInteractable = j.value("IsInteractable", true);
	return true;
}
