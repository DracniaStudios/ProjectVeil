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
	auto rng = std::ranlux24_base(std::random_device{}());
	defaultColor = getRandomColor(rng);

	//AudioManager::getInstance().Play("anime_wow");
	AudioManager::getInstance().Play3D(defaultSound, *this);
	// Play3D only populates soundInstance when it falls back to the FMOD Studio event path;
	// a plain loaded sound (the common case) leaves it null.
	if (soundInstance != nullptr) {
		soundInstance->setParameterByName(soundParameterName.c_str(), soundParameterValue);
	}

	if (interactType == INTERACT_MINIGAME)
	{
		auto scene = SceneManager::getInstance().currentScene;
		scene->SetMiniGame(interactValue);
		scene->player->interactObject = scene->gameMap.FindGameObjectByID(activatorValue);
		isRunningMiniGame = true;
		return;
	}

	if (interactType == INTERACT_ITEM)
	{
		// Give Player Specific Item
		return;
	}
}
