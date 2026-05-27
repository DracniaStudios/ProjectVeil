#pragma once
#ifndef DEVELOPER_WINDOW_H
#define DEVELOPER_WINDOW_H

struct SceneManager;
struct Player;


struct DeveloperWindow
{
	
	void render(SceneManager* manager);
	void update(SceneManager* manager, Player* player);

	bool isPlayerActive = false;
	bool isCameraActive = false;
	bool isInspectorActive = false;
	bool isMiniGameActive = false;
	bool showCursorPosition = false;

	void showPlayerData(SceneManager* manager, Player* player);
	void showCameraData(SceneManager* manager, Player* player);
	void showObjectInspector(SceneManager* manager);
	void showMiniGameData(SceneManager* manager, Player* player);

};

#endif
