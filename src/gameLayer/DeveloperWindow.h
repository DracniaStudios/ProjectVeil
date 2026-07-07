#pragma once
#ifndef DEVELOPER_WINDOW_H
#define DEVELOPER_WINDOW_H

#include <SceneManager.h>
#include <Player.h>

inline const char* boolToString(bool boolean)
{
	if (boolean) { return "True"; }
	return "false";
}

class DeveloperWindow
{
	DeveloperWindow() = default; // Private constructor to prevent instantiation
	
	// Tool Tip
	bool showCursorPosition = false;
	
	/** Window Flags **/
	bool isActive = false;
	bool isPlayerActive = false;
	bool isCameraActive = false;
	bool isInspectorActive = false;
	bool isEntityActive = false;
	bool isMiniGameActive = false;
	bool isGameDataActive = false;
	bool isAssetActive = false;

	// Object & Entity Inspector Data

	/** Object Inspector Data **/
	GameObject* inspectObject;
	Entity* inspectEntity;

	GameObject* newObject;
	Entity* newEntity;
	Vector4 colorHolder = Vector4(0, 0, 0, 255);
	char inputName[128] = "New Object";
	bool isCreatingObject = false;
	bool isCreatingEntity = false;

	// Mini Game Inspector Data
	int currentGameID = 0;
	void* miniGameObject;

	/** Window Data **/
	void ShowGameData(Player* player);
	void ShowPlayerData(Player* player);
	void ShowCameraData(Player* player);
	void ShowObjectInspector();
	void ShowEntityInspector();
	void ShowMiniGameData(Player* player);
	void ShowAssetData();
public:
	// Singleton Pattern Implementation
	DeveloperWindow(const DeveloperWindow&) = delete;
	DeveloperWindow& operator=(const DeveloperWindow&) = delete;
	DeveloperWindow(DeveloperWindow&&) = delete;
	DeveloperWindow& operator=(DeveloperWindow&&) = delete;
	
	static DeveloperWindow& getInstance() {
		static DeveloperWindow instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	/** Functions **/
	void update(Player* player);
	void ShowInspector(int inspectorID, bool isEnabled = true);

	bool IsEnabled() const { return isActive; }
};

#endif
