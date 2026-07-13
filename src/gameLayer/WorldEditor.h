#pragma once
#ifndef WORLD_EDITOR_H
#define WORLD_EDITOR_H

#include <SceneManager.h>
#include <Player.h>
#include <EditorUtils.h>

class WorldEditor
{
	WorldEditor() = default; // Private constructor to prevent instantiation

	/** Window Flags **/
	bool isEditorActive = false; // Master switch (F8)
	bool isWorldSettingsActive = true;
	bool isObjectBrowserActive = false;
	bool isPlacementActive = false;
	bool isPaletteActive = false;

	/** Selection State **/
	std::uint64_t selectedObjectId = 0; // Id instead of pointer — survives gameObjects reallocation
	int activeTextureIndex = -1; // Index into AssetManager::assets

	/** Placement State **/
	GameObject stagingObject = {};
	char inputName[128] = "New Block";
	Vector4 colorHolder = Vector4(255, 255, 255, 255);

	/** Feedback **/
	std::string statusMessage;

	/** Window Data **/
	void ShowEditorHub();
	void ShowWorldSettings();
	void ShowObjectBrowser();
	void ShowPlacementPanel();
	void ShowTexturePalette();

	/** Helpers **/
	GameObject* getSelectedObject();
	Asset* getActiveTexture();
public:
	// Singleton Pattern Implementation
	WorldEditor(const WorldEditor&) = delete;
	WorldEditor& operator=(const WorldEditor&) = delete;
	WorldEditor(WorldEditor&&) = delete;
	WorldEditor& operator=(WorldEditor&&) = delete;

	static WorldEditor& getInstance() {
		static WorldEditor instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	/** Functions **/
	void update(Player* player);

	bool IsEnabled() const { return isEditorActive; }
};

#endif
