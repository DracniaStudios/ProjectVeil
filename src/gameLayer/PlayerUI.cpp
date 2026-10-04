#include "PlayerUI.h"

#include <SceneManager.h>

void PlayerUI::update() {
	// Update the UI elements based on the player's state
}

void PlayerUI::render() {
	if (!isVisible) return; // Skip rendering if the UI is not visible

	// Render the UI elements on the screen

	if (addButton("Return", Rectangle{ 0, 0, 100, 50 }, Rectangle{ 0.0f, 0.0f, 0.05f, 0.05f }, UIEngine::getInstance())) {
		SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), 0);
	}

}