#pragma once
#ifndef DEVELOPER_WINDOW_H
#define DEVELOPER_WINDOW_H

struct SceneManager;
struct AssetManager;
struct Player;
struct GameObject;

struct DeveloperWindow
{
	/** Functions **/
	void render(SceneManager* manager);
	void update(SceneManager* manager, AssetManager* assetManager, Player* player);

	/** Window Flags **/
	bool isPlayerActive = false;
	bool isCameraActive = false;
	bool isInspectorActive = false;
	bool isMiniGameActive = false;
	bool showCursorPosition = false;

	/** Window Data **/
	void showPlayerData(SceneManager* manager, Player* player);
	void showCameraData(SceneManager* manager, Player* player);
	void showObjectInspector(SceneManager* manager, AssetManager* assetManager);
	void showMiniGameData(SceneManager* manager, Player* player);

	/** Object Inspector Data **/
	void* inspectObject;
	void* newObject;

};

#endif
