#include "PlayerUI.h"

#include <SceneManager.h>

void PlayerUI::update() {
	// Update the UI elements based on the player's state
}

void PlayerUI::render() {
	if (!isVisible) return; // Skip rendering if the UI is not visible

	// Render the UI elements on the screen

	// Fixed size in the top-left corner; grows with the window through the UI scale
	if (UI::drawButton("Return", Rectangle{ 20, 20, 180, 50 })) {
		SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), 0);
	}

}