#pragma once
#ifndef DEVELOPER_WINDOW_H
#define DEVELOPER_WINDOW_H

struct Player;
struct GameObject;

struct DeveloperWindow
{
private:
	DeveloperWindow() = default; // Private constructor to prevent instantiation
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

	/** Window Flags **/
	bool isPlayerActive = false;
	bool isCameraActive = false;
	bool isInspectorActive = false;
	bool isMiniGameActive = false;
	bool isGameDataActive = false;
	bool showCursorPosition = false;

	/** Window Data **/
	void showGameData(Player* player);
	void showPlayerData(Player* player);
	void showCameraData(Player* player);
	void showObjectInspector();
	void showMiniGameData(Player* player);

	/** Object Inspector Data **/
	void* inspectObject;
	void* newObject;

};

#endif
