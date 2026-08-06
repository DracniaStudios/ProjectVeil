#include <GameObject.h>

#include <SceneManager.h>
#include <gameMap.h>
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

	if (interactType == INTERACT_MINIGAME)
	{
		scene->SetMiniGame(interactValue);
		// Set Activator Object else self.
		scene->player->interactObject = activatorValue != 0 ? scene->gameMap.FindGameObjectByID(activatorValue) : this;
		isRunningMiniGame = true;
		return;
	}
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
