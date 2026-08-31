#pragma once
#ifndef WORLD_EDITOR_H
#define WORLD_EDITOR_H

#include <SceneManager.h>
#include <Player.h>
#include <helpers.h>

#include <WorldEditorTools/EditorGizmo.h>
#include <WorldEditorTools/EditorPicking.h>

#include <vector>

struct EditorCamera : Camera3D {
	Vector3 forward = {};
	Vector3 back = {};
	Vector3 right = {};
	Vector3 left = {};
	Vector3 up = {};
	Vector3 down = {};
	Vector2 sensitivity = Vector2{ 0.01f, 0.01f };
	Vector2 lookRotation = {};

	// Look angles live on the camera rather than as function-local statics, so
	// they can be derived from whatever camera the editor takes over from
	// instead of starting at zero and snapping the view somewhere arbitrary.
	float yaw = 0.0f;
	float pitch = 0.0f;
	bool isSeeded = false;

	void Update(Camera3D* camera);

	// Adopts a pose from the camera the editor is replacing. Called once when
	// the editor opens; without it the first F1 teleports the view to the origin
	// because EditorCamera default-constructs its own position at {0,0,0}.
	void SyncFrom(const Camera3D& camera);

	// Frames a point at a given distance, keeping the current look direction.
	void FocusOn(Vector3 target, float distance);
};

/**
 * One reversible edit.
 *
 * Transform edits store the values from *before* the change; spawns store only
 * the id, and undo by deleting it. Deletion is deliberately not undoable —
 * resurrecting a polymorphic object from a GameObject copy is exactly the
 * slicing bug this codebase has already had to fix once, and a silently wrong
 * restore is worse than no restore.
 */
struct EditorEdit
{
	enum Kind { EDIT_TRANSFORM, EDIT_SPAWN };

	Kind kind = EDIT_TRANSFORM;
	std::uint64_t id = 0;
	Vector3 position = {};
	Quaternion rotation = QuaternionIdentity();
	Vector3 scale = Vector3One();
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
	bool isLightingActive = false;
	bool isStalkerActive = false;

	/** World Settings **/
	std::string worldName = "world";

	/** Selection State **/
	std::uint64_t selectedObjectId = 0; // Id instead of pointer — survives gameObjects reallocation
	int activeTextureIndex = -1; // Index into AssetManager::assets
	int activeModelIndex = -1; // Index into AssetManager::assets

	/** Viewport Manipulation **/
	EditorGizmo gizmo = {};

	// Physics is running when the editor is open, so pausing will allow testing/adjusting objects correctly without gravity.
	bool simulationPaused = true;

	// Bounded so a long session cannot grow the history without limit
	static constexpr size_t UNDO_LIMIT = 128;
	std::vector<EditorEdit> undoStack = {};
	EditorEdit pendingEdit = {}; // Captured on drag start, pushed on drag end

	/** Placement State **/
	// Which kind of object the Placement Panel spawns
	enum PlacementKind { PLACE_GAME_OBJECT, PLACE_ENTITY, PLACE_INTERACTABLE };
	int placementKind = PLACE_GAME_OBJECT;

	/** Point & Place State **/
	bool placementMode = false;        // Armed: the mouse drops objects into the world
	bool placeAlignToSurface = true;   // Rest the object on the surface it lands on
	bool placementSnap = true;
	float placementGridStep = 1.0f;

	// Recomputed every frame from the mouse ray, consumed by DrawViewport3D
	bool placementPreviewValid = false;
	Vector3 placementPreviewPosition = {};
	Vector3 placementPreviewNormal = { 0.0f, 1.0f, 0.0f };

	// Object Browser Viewability
	bool showGameObjects = false;
	bool showEntities = false;
	bool showInteractables = false;

	// Staging Object Tools
	GameObject stagingObject = {};
	char inputName[128] = "New Block";
	Vector4 colorHolder = Vector4(255, 255, 255, 255);

	// Entity extras, applied on spawn when placing an Entity
	// Which concrete Entity subclass the Placement Panel spawns. Indexes
	// EntityKind directly, so the combo order below must match that enum.
	int stagingEntityKind = ENTITYKIND_NONE;
	float stagingMaxHealth = 10.0f;
	float stagingMaxStamina = 100.0f;
	float stagingBaseSpeed = 1.0f;

	// Interactable extras, applied on spawn when placing an Interactable
	int stagingInteractType = INTERACT_NONE;
	int stagingInteractValue = 0;
	int stagingActivatorValue = 0;

	/** Object & Entity Inspector State **/
	std::uint64_t inspectEntityId = 0; // Id instead of pointer — survives entity removal (gameplay death, Load Game)

	// Rotation is stored as a quaternion but edited as Euler degrees. The typed
	// values are cached rather than re-derived each frame: QuaternionToEuler is
	// discontinuous (180 flips to -180, gimbal lock collapses two axes), so a
	// live round-trip would make the fields jump around under the user's cursor.
	std::uint64_t rotationEulerOwnerId = 0;   // Object the cached angles belong to
	Vector3 rotationEulerDegrees = {};        // What the widget shows
	Quaternion rotationEulerSource = {};      // Quaternion those angles produced

	/** Mini Game Inspector State **/
	int currentGameID = 0;
	int miniGameObstacleIndex = -1; // Index instead of pointer — obstacles vector reallocates during play

	/** Lighting Inspector State **/
	int selectedLightIndex = -1; // Index instead of pointer — the lights vector reallocates on add/remove

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
	void ShowLightingData();
	// Stalker AI: FSM state, what it last heard, and the Director's hint log.
	// The hint log is the artifact the slice's acceptance criteria are checked
	// against — "verify by logging what the Director passes".
	void ShowStalkerData();

	/** Viewport Interaction (EditorViewport.cpp) **/
	// Arbitrates the mouse between ImGui, the camera, the gizmo, placement and
	// selection — in that order. The ordering is the whole design; see the
	// implementation for why each step has to come where it does.
	void UpdateViewportInput();
	void UpdateHotkeys();
	void UpdatePlacementPreview(Scene* scene, const Camera3D& camera, bool canPlace);

	// Writes a manipulated transform back onto a body, keeping every derived
	// value (collision box, lastPosition, velocities) consistent with it
	void ApplyTransform(GameObject* object, const GizmoTransform& transform);

	void PushEdit(const EditorEdit& edit);
	void Undo();

	/** Selection Commands **/
	void SelectUnderMouse(Scene* scene, const Camera3D& camera);
	void DeleteSelection();
	void DuplicateSelection();
	void FocusOnSelection();
	void SnapSelectionToGrid();
	// Drops selection, any in-flight drag, and the edit history together. Ids
	// from a previous world do not survive a load, so neither may anything
	// holding them.
	void ResetSelectionState();

	/** Helpers **/
	GameObject* getSelectedObject();
	Asset* getActiveTexture();
	Asset* getActiveModel();
	GizmoTransform getSelectedTransform(const GameObject* object) const;
	// Spawns a copy of stagingObject at a world position, honouring placementKind.
	// Shared by the Placement Panel's buttons and by point-and-place.
	GameObject* SpawnStagedObject(Vector3 position);
	void showGameObject(GameObject* object);
	void showEntity(Entity* object);
	void showInteractableObject(InteractableObject* object);
	void showTransformTools();



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

	// Selection outline, transform gizmo and placement ghost. Must be called
	// inside BeginMode3D — SceneManager_draw does it after the light gizmos.
	void DrawViewport3D();

	bool IsEnabled() const { return isEditorActive; }

	// Scene_updateScene asks this before stepping physics. Only meaningful while
	// the editor is open; the caller checks IsEnabled() as well.
	bool IsSimulationPaused() const { return simulationPaused; }

	void SelectObject(std::uint64_t id) { selectedObjectId = id; }
};

#endif
