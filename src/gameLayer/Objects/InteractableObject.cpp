#include <GameObject.h>

#include <SceneManager.h>
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
		SceneManager::getInstance().currentScene->SetMiniGame(interactValue);
		isRunningMiniGame = true;
		return;
	}

	if (interactType == INTERACT_ITEM)
	{
		// Give Player Specific Item
		return;
	}
}

void InteractableObject::update(Scene* scene, float delta) {

	GameObject::update(scene, delta);
	if (!isRunningMiniGame) return;

	if (auto miniGame = scene->miniGame; scene->isMiniActive && scene->GetLastMiniGame() == interactValue) {

		if (miniGame->data->isComplete) {
			auto object = &scene->gameMap.gameObjects[interactValue];
			if (object->isEnabled) { object->onDisable(); std::cout << "Disable Object: " << interactValue;}
			if (!object->isEnabled) { object->onEnable(); std::cout << "Enable Object: " << interactValue;}
			isRunningMiniGame = false;
		}
	}

}