#pragma once
#ifndef DEVELOPER_WINDOW_H
#define DEVELOPER_WINDOW_H

#include <raylib.h>
#include <imgui.h>

struct SceneManager;
struct Player;


struct DeveloperWindow
{
	
	void update();
	void render(SceneManager* manager, Player* player);

	bool isPlayerActive = false;
	bool isCameraActive = false;
	bool isInspectorActive = false;
	bool isMiniGameActive = false;

	void showPlayerData(SceneManager* manager, Player* player);
	void showCameraData(SceneManager* manager, Player* player);
	void showObjectInspector(SceneManager* manager);
	void showMiniGameData(SceneManager* manager, Player* player);

};

#endif
