#include <GameObject.h>

#include <SceneManager.h>
InteractableObject::InteractableObject(const InteractionType interact, int value)
{
	interactType = interact;
	interactValue = value;
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
		SceneManager::getInstance().currentScene->SetMiniGame(interactValue);
		return;
	}

	if (interactType == INTERACT_OBJECT)
	{
		// Activate Specific Object ID
		return;
	}

	if (interactType == INTERACT_ITEM)
	{
		// Give Player Specific Item
		return;
	}
}