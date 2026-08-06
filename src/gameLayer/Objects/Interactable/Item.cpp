#include "Item.h"

#include <SceneManager.h>

Item::Item() {
	type = ITEM_NONE;
	id = 0;
	quantity = 1;

	interactType = INTERACT_ITEM;
}

void Item::OnInteract() {
	InteractableObject().onInteract();
	auto scene = SceneManager::getInstance().currentScene;

	if (interactValue == 0) {
		interactValue = id;
	}

	if (interactType == INTERACT_ITEM) {
		if (scene->player->data.inventory.AddItem(*this)) {
			scene->gameMap.removeInteractable(this);
			return;
		}
	}
}