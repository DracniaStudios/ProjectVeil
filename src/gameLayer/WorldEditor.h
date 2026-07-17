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
	bool isHierarchyActive = false;

	/** Developer Tool Flags **/
	bool isGameDataActive = false;
	bool isPlayerActive = false;
	bool isCameraActive = false;
	bool isInspectorActive = false;
	bool isEntityActive = false;
	bool isMiniGameActive = false;
	bool isAssetActive = false;

	/** Selection State **/
	std::uint64_t selectedObjectId = 0; // Id instead of pointer — survives gameObjects reallocation
	int activeTextureIndex = -1; // Index into AssetManager::assets
	int activeModelIndex = -1; // Index into AssetManager::assets

	/** Placement State **/
	GameObject stagingObject = {};
	char inputName[128] = "New Block";
	Vector4 colorHolder = Vector4(255, 255, 255, 255);

	/** Object & Entity Inspector State **/
	std::uint64_t inspectObjectId = 0; // Ids instead of pointers — survive gameObjects reallocation
	std::uint64_t inspectEntityId = 0; // and entity removal (gameplay death, Load Game)

	GameObject* newObject = nullptr;
	Entity* newEntity = nullptr;
	Vector4 createColorHolder = Vector4(0, 0, 0, 255);
	char createName[128] = "New Object";
	bool isCreatingObject = false;
	bool isCreatingEntity = false;

	/** Mini Game Inspector State **/
	int currentGameID = 0;
	int miniGameObstacleIndex = -1; // Index instead of pointer — obstacles vector reallocates during play

	/** Feedback **/
	std::string statusMessage;

	/** Window Data **/
	void ShowEditorHub();
	void ShowWorldSettings();
	void ShowObjectBrowser();
	void ShowPlacementPanel();
	void ShowHierarchy();

	/** Developer Tool Windows **/
	void ShowGameData(Player* player);
	void ShowPlayerData(Player* player);
	void ShowCameraData(Player* player);
	void ShowObjectInspector();
	void ShowEntityInspector();
	void ShowMiniGameData(Player* player);
	void ShowAssetData();

	/** Helpers **/
	GameObject* findGameObject(std::uint64_t id);
	Entity* findEntity(std::uint64_t id);
	GameObject* getSelectedObject();
	Asset* getActiveTexture();
	Asset* getActiveModel();
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
