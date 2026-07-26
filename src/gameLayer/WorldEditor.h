#pragma once
#ifndef WORLD_EDITOR_H
#define WORLD_EDITOR_H

#include <SceneManager.h>
#include <Player.h>
#include <EditorUtils.h>

struct EditorCamera : Camera3D {
	Vector3 forward = {};
	Vector3 back = {};
	Vector3 right = {};
	Vector3 left = {};
	Vector3 up = {};
	Vector3 down = {};
	Vector2 sensitivity = Vector2{ 0.01f, 0.01f };
	Vector2 lookRotation = {};

	void Update(Camera3D* camera);
};

class WorldEditor
{
	WorldEditor() = default; // Private constructor to prevent instantiation

	/** Window Flags **/
	bool isEditorActive = false; // Master switch (F1)
	bool isWorldSettingsActive = true;
	bool isObjectBrowserActive = false;
	bool isPlacementActive = false;

	bool isPlayerActive = false;
	bool isCameraActive = false;
	bool isEntityActive = false;
	bool isMiniGameActive = false;
	bool isAssetActive = false;

	/** Selection State **/
	std::uint64_t selectedObjectId = 0; // Id instead of pointer — survives gameObjects reallocation
	int activeTextureIndex = -1; // Index into AssetManager::assets
	int activeModelIndex = -1; // Index into AssetManager::assets

	/** Placement State **/
	// Which kind of object the Placement Panel spawns
	enum PlacementKind { PLACE_GAME_OBJECT, PLACE_ENTITY, PLACE_INTERACTABLE };
	int placementKind = PLACE_GAME_OBJECT;

	GameObject stagingObject = {};
	char inputName[128] = "New Block";
	Vector4 colorHolder = Vector4(255, 255, 255, 255);

	// Entity extras, applied on spawn when placing an Entity
	float stagingMaxHealth = 10.0f;
	float stagingMaxStamina = 100.0f;
	float stagingBaseSpeed = 1.0f;

	// Interactable extras, applied on spawn when placing an Interactable
	int stagingInteractType = INTERACT_NONE;
	int stagingInteractValue = 0;
	int stagingActivatorValue = 0;

	/** Object & Entity Inspector State **/
	std::uint64_t inspectEntityId = 0; // Id instead of pointer — survives entity removal (gameplay death, Load Game)

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

	/** Developer Tool Windows **/
	void ShowPlayerData(Player* player);
	void ShowCameraData(Player* player);
	void ShowMiniGameData(Player* player);
	void ShowAssetData();

	/** Helpers **/
	GameObject* findGameObject(std::uint64_t id);
	Entity* findEntity(std::uint64_t id);
	InteractableObject* findInteractable(std::uint64_t);
	GameObject* getSelectedObject();
	Asset* getActiveTexture();
	Asset* getActiveModel();
	void showGameObject(GameObject* object);
	void showEntity(Entity* object);
	void showInteractableObject(InteractableObject* object);



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

	EditorCamera editorCamera = {};
	
	/** Functions **/
	void update(Player* player);

	bool IsEnabled() const { return isEditorActive; }
	void SelectObject(std::uint64_t id) { selectedObjectId = id; }
};

#endif
