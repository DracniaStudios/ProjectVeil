#include <GameObject.h>

#include <SceneManager.h>
#include <gameMap.h>


void ActivateMiniGame(InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Activating MiniGame: " << interactable->interactValue << "\n";
	auto scene = SceneManager::getInstance().currentScene;
	scene->SetMiniGame(interactable->interactValue);
	// Set Activator Object else self.
	scene->player->interactObject = interactable->activatorValue != 0 ? scene->gameMap.FindGameObjectByID(interactable->activatorValue) : interactable;
	interactable->isRunningMiniGame = true;
}

void UnlockMiniGame(InteractableObject* interactable)
{
	std::cout << "[InteractableObject] Unlocking MiniGame: " << interactable->interactValue << "\n";
	auto scene = SceneManager::getInstance().currentScene;
	scene->player->artifactUnlocked = std::max(scene->player->artifactUnlocked, interactable->interactValue);
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
	auto scene = SceneManager::getInstance().currentScene;

	// Play3D only populates soundInstance when it falls back to the FMOD Studio event path;
	// a plain loaded sound (the common case) leaves it null.
	if (soundInstance != nullptr) {
		soundInstance->setParameterByName(soundParameterName.c_str(), soundParameterValue);
	}

	std::cout << "[InteractObject] Interacted with " << name << "\n";

	if (interactType == INTERACT_MINIGAME) { ActivateMiniGame(this); }
	if (interactType == INTERACT_UNLOCK) { UnlockMiniGame(this); }
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
