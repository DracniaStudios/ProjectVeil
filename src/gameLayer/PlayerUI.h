#pragma once
#ifndef PLAYERUI_H
#define PLAYERUI_H

#include <UIEngine.h>

struct PlayerUI {
private:
	PlayerUI() = default;

public:
	// Delete, copy, and move functions to prevent duplication
	PlayerUI(const PlayerUI&) = delete;
	PlayerUI& operator=(const PlayerUI&) = delete;
	PlayerUI(PlayerUI&&) = delete;
	PlayerUI& operator=(PlayerUI&&) = delete;

	// Global Access point to the UIEngine instance
	static PlayerUI& getInstance() {
		static PlayerUI instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	bool isVisible = true; // Flag to control the visibility of the UI

	void update();
	void render();

};

#endif // PLAYERUI_H
