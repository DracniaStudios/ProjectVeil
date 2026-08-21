#include "WorldEditor.h"

#include <rlgl.h>

#include <cmath>
#include <string>

namespace
{
	// Selection outline, drawn slightly larger than the object so it does not
	// z-fight with the model's own wireframe pass in GameObject::render3D.
	constexpr float kOutlineInflate = 1.02f;
	const Color kSelectionColor = Color{ 255, 190, 60, 255 };
	const Color kPlacementColor = Color{ 90, 220, 160, 255 };

	/**
	 * Resolves an id across all three containers.
	 *
	 * Objects are addressed by id and never by pointer anywhere in the editor:
	 * GameMap::saveObject() push_backs into a vector and then re-sorts it, so a
	 * single spawn invalidates every outstanding GameObject* into gameObjects.
	 */
	GameObject* FindAnyById(Scene* scene, std::uint64_t id)
	{
		if (scene == nullptr || id == 0) { return nullptr; }
		if (GameObject* interactable = scene->gameMap.FindInteractableByID(id)) { return interactable; }
		if (GameObject* entity = scene->gameMap.FindEntityByID(id)) { return entity; }
		return scene->gameMap.FindGameObjectByID(id);
	}

	/** Removes by id from whichever container actually owns the object. */
	bool RemoveById(Scene* scene, std::uint64_t id)
	{
		if (scene == nullptr || id == 0) { return false; }

		if (scene->interactables.contains(id))
		{
			scene->gameMap.removeInteractable(scene->interactables[id].get());
			return true;
		}
		if (scene->entities.contains(id))
		{
			scene->gameMap.removeEntity(scene->entities[id].get());
			return true;
		}
		if (GameObject* object = scene->gameMap.FindGameObjectByID(id))
		{
			scene->gameMap.removeObject(object);
			return true;
		}
		return false;
	}
}

#pragma region Input Arbitration

/**
 * One mouse, five consumers. The order below is the design:
 *
 *  1. ImGui — a click that lands on a panel is never also a world click.
 *  2. Camera — the right button belongs to look, and look must never run in the
 *     same frame as a manipulation or the object chases the view.
 *  3. Placement — while armed it owns the left button outright.
 *  4. Gizmo — tested before selection, so grabbing a handle that happens to sit
 *     over another object does not reselect that object instead.
 *  5. Selection — the fallback for a click that nothing else claimed.
 *
 * Getting 4 and 5 the wrong way round is the classic editor bug: every attempt
 * to drag a handle silently reselects whatever is behind it.
 */
void WorldEditor::UpdateViewportInput()
{
	const auto manager = &SceneManager::getInstance();
	Scene* scene = manager->currentScene;
	if (scene == nullptr) { return; }

	const Camera3D& camera = manager->camera3D;
	const ImGuiIO& io = ImGui::GetIO();

	// Step 1 & 2. Without the ImGui test, pressing "Delete Object" in the Object
	// Browser also fires a world pick through the panel, and the selection
	// changes under the button that was just clicked.
	const bool mouseOverViewport = !io.WantCaptureMouse;
	const bool cameraLooking = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
	const bool canGrab = mouseOverViewport && !cameraLooking;

	// Step 3.
	if (placementMode)
	{
		// A half-finished drag must not survive into placement mode: the gizmo
		// would keep writing to an object the user is no longer looking at.
		gizmo.Cancel();
		UpdatePlacementPreview(scene, camera, !cameraLooking);
		return;
	}
	placementPreviewValid = false;

	// Step 4. Resolved fresh every frame — see FindAnyById on why nothing here
	// may cache the pointer across a spawn or delete.
	bool gizmoOwnsMouse = false;
	if (GameObject* selected = getSelectedObject())
	{
		GizmoTransform transform = getSelectedTransform(selected);

		gizmoOwnsMouse = gizmo.Update(camera, transform, canGrab);

		if (gizmo.DragBegan())
		{
			const GizmoTransform& start = gizmo.DragStartTransform();
			pendingEdit = EditorEdit{ EditorEdit::EDIT_TRANSFORM, selected->id,
				start.position, start.rotation, start.scale };
		}

		// Only write back while the gizmo is actually driving. Writing every
		// frame would clear the body's velocity continuously, which silently
		// pins every selected object in place whenever the simulation is live.
		if (gizmo.IsDragging() || gizmo.DragEnded()) { ApplyTransform(selected, transform); }

		if (gizmo.DragEnded() && pendingEdit.id == selected->id)
		{
			// A click that grabbed a handle but moved nothing is not an edit,
			// and pushing it would make the first Ctrl+Z appear to do nothing.
			const bool changed =
				!Vector3Equals(pendingEdit.position, transform.position) ||
				!Vector3Equals(pendingEdit.scale, transform.scale) ||
				!QuaternionEquals(pendingEdit.rotation, transform.rotation);

			if (changed) { PushEdit(pendingEdit); }
		}
	}

	// Step 5.
	//canGrab
	if (gizmo.mode == GIZMO_SELECT && !gizmoOwnsMouse && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		SelectUnderMouse(scene, camera);
	}
}

/**
 * Keyboard tools.
 *
 * Every binding is bare-key or Ctrl-prefixed, and the two sets never mix: the
 * Ctrl branch returns rather than falling through, so Ctrl+2 toggles the Object
 * Browser without also switching the gizmo to Move.
 */
void WorldEditor::UpdateHotkeys()
{
	const ImGuiIO& io = ImGui::GetIO();
	auto& gameObjects = SceneManager::getInstance().currentScene->gameMap.gameObjects;

	// Typing into an ImGui field must not also drive the editor. Without this,
	// naming an object "Duplicate" cycles tools and deletes the selection while
	// the letters are still being typed.
	if (io.WantCaptureKeyboard) { return; }

	if (IsKeyDown(KEY_LEFT_CONTROL))
	{
		if (IsKeyPressed(KEY_ONE)) { isWorldSettingsActive = !isWorldSettingsActive; }
		if (IsKeyPressed(KEY_TWO)) { isObjectBrowserActive = !isObjectBrowserActive; 
			std::sort(gameObjects.begin(), gameObjects.end(), [](const GameObject& a, const GameObject& b) {
				return a.id > b.id;
			});
		}
		if (IsKeyPressed(KEY_THREE)) { isPlacementActive = !isPlacementActive; }
		if (IsKeyPressed(KEY_FOUR)) { isPlayerActive = !isPlayerActive; }
		if (IsKeyPressed(KEY_FIVE)) { isCameraActive = !isCameraActive; }
		if (IsKeyPressed(KEY_SIX)) { isMiniGameActive = !isMiniGameActive; }
		if (IsKeyPressed(KEY_SEVEN)) { isAssetActive = !isAssetActive; }
		if (IsKeyPressed(KEY_EIGHT)) { isLightingActive = !isLightingActive; }

		if (IsKeyPressed(KEY_Z)) { Undo(); }
		if (IsKeyPressed(KEY_D)) { DuplicateSelection(); }
		return;
	}

	// Tool selection. 1-4 are free as bare keys — the panel shortcuts above all
	// require Ctrl.
	if (IsKeyPressed(KEY_ONE)) { gizmo.mode = GIZMO_SELECT; statusMessage = "Tool: Select"; }
	if (IsKeyPressed(KEY_TWO)) { gizmo.mode = GIZMO_TRANSLATE; statusMessage = "Tool: Move"; }
	if (IsKeyPressed(KEY_THREE)) { gizmo.mode = GIZMO_ROTATE; statusMessage = "Tool: Rotate"; }
	if (IsKeyPressed(KEY_FOUR)) { gizmo.mode = GIZMO_SCALE; statusMessage = "Tool: Scale"; }

	if (IsKeyPressed(KEY_P))
	{
		placementMode = !placementMode;
		// Arming the tool without showing what it will spawn is a trap of its
		// own, so the panel comes along with it.
		if (placementMode) { isPlacementActive = true; }
		statusMessage = placementMode ? "Point & Place armed" : "Point & Place off";
	}

	if (IsKeyPressed(KEY_G))
	{
		gizmo.snapEnabled = !gizmo.snapEnabled;
		statusMessage = gizmo.snapEnabled ? "Snap on" : "Snap off";
	}

	if (IsKeyPressed(KEY_X))
	{
		gizmo.localSpace = !gizmo.localSpace;
		statusMessage = gizmo.localSpace ? "Space: Local" : "Space: World";
	}

	if (IsKeyPressed(KEY_DELETE)) { DeleteSelection(); }
	if (IsKeyPressed(KEY_F)) { FocusOnSelection(); }
}

#pragma endregion

#pragma region Placement

/**
 * Point-and-place preview and commit.
 *
 * The ray is queried with PICK_SURFACE rather than PICK_SELECTABLE: the floor
 * has canBeSelected cleared by GameMap::create(), and it is also the single most
 * common thing to drop an object onto. Using the selection filter here makes the
 * ground invisible to placement and every object lands on the fallback plane.
 */
void WorldEditor::UpdatePlacementPreview(Scene* scene, const Camera3D& camera, bool canPlace)
{
	placementPreviewValid = false;

	// ImGui uses Escape to revert an active text field, so it only means "cancel
	// placement" when no widget has the keyboard.
	if (IsKeyPressed(KEY_ESCAPE) && !ImGui::GetIO().WantCaptureKeyboard)
	{
		placementMode = false;
		statusMessage = "Point & Place cancelled";
		return;
	}

	const Ray ray = GetEditorMouseRay(camera);

	Vector3 point = {};
	Vector3 normal = { 0.0f, 1.0f, 0.0f };

	if (const PickResult surface = PickSceneObject(scene, ray, PICK_SURFACE); surface.hit)
	{
		point = surface.point;
		normal = surface.normal;
	}
	else if (!RayGroundPlane(ray, 0.0f, point))
	{
		// Aimed above the horizon with nothing in the way. There is no defined
		// position, so there is nothing to preview and nothing to place —
		// RayPlane already refused rather than returning a NaN.
		return;
	}

	// The staging object starts life with a zeroed scale; saveObject() repairs
	// that on spawn, but the preview has to agree with the object that will
	// actually appear or the ghost sits half-buried. A typed-in negative extent
	// would additionally push the object *into* the surface below, so it is
	// clamped rather than merely zero-checked.
	Vector3 size = stagingObject.rigidBody3D.scale;
	if (size == Vector3Zero()) { size = Vector3One(); }
	size = SanitizeScale(size);

	if (placeAlignToSurface)
	{
		// Push out along the hit normal by the object's own extent in that
		// direction, so it rests on the surface instead of straddling it.
		const float extent =
			fabsf(normal.x) * size.x +
			fabsf(normal.y) * size.y +
			fabsf(normal.z) * size.z;
		point = Vector3Add(point, Vector3Scale(normal, extent * 0.5f));
	}

	if (placementSnap && placementGridStep > 0.0f)
	{
		// Snap across the surface, never into it. Rounding the axis the object
		// is resting on would sink it back through the floor it was just lifted
		// clear of — the alignment above and a naive 3-axis snap fight directly.
		Vector3 snapped = SnapVector(point, placementGridStep);
		if (fabsf(normal.x) > 0.5f) { snapped.x = point.x; }
		if (fabsf(normal.y) > 0.5f) { snapped.y = point.y; }
		if (fabsf(normal.z) > 0.5f) { snapped.z = point.z; }
		point = snapped;
	}

	placementPreviewPosition = point;
	placementPreviewNormal = normal;
	placementPreviewValid = true;

	if (canPlace && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		if (GameObject* spawned = SpawnStagedObject(point))
		{
			EditorEdit edit = {};
			edit.kind = EditorEdit::EDIT_SPAWN;
			edit.id = spawned->id;
			PushEdit(edit);
		}
		// Stays armed: levels are built by placing many objects in a row, and
		// re-arming per object would make the tool unusable.
	}
}

#pragma endregion

#pragma region Selection Commands

void WorldEditor::SelectUnderMouse(Scene* scene, const Camera3D& camera)
{
	const PickResult result = PickSceneObject(scene, GetEditorMouseRay(camera), PICK_SELECTABLE);

	if (!result.hit)
	{
		selectedObjectId = 0;
		gizmo.Cancel();
		statusMessage = "Selection cleared";
		return;
	}

	selectedObjectId = result.id;
	statusMessage = "Selected: " + (result.object != nullptr ? result.object->name : std::string("object"));
}

void WorldEditor::DeleteSelection()
{
	Scene* scene = SceneManager::getInstance().currentScene;
	GameObject* object = getSelectedObject();
	if (scene == nullptr || object == nullptr) { statusMessage = "Nothing selected"; return; }

	// Drop the grab before the object goes away: a drag that outlived its target
	// would finish by writing a transform through a dangling pointer.
	gizmo.Cancel();

	const std::string name = object->name;
	const std::uint64_t id = object->id;

	// RemoveById re-resolves the object itself rather than trusting the pointer
	// above, which erase_if would otherwise compare by address.
	if (RemoveById(scene, id)) { statusMessage = "Deleted: " + name; }
	else { statusMessage = "Delete failed: " + name; }

	selectedObjectId = 0;
}

void WorldEditor::DuplicateSelection()
{
	Scene* scene = SceneManager::getInstance().currentScene;
	GameObject* object = getSelectedObject();
	if (scene == nullptr || object == nullptr) { statusMessage = "Nothing selected"; return; }

	gizmo.Cancel();

	// Copying through the GameObject base would slice off an Entity's or
	// Interactable's extra data while leaving copy.type claiming to be one, so
	// a later lookup by that type would index a container the copy was never
	// inserted into. Each kind is therefore copied through its own type.
	GameObject* spawned = nullptr;
	const Vector3 offset = Vector3{ 1.0f, 0.0f, 0.0f };

	//
	// Each copy also inherits the source's model handle and its ownsModel flag.
	// The original is still live and still using that model, so the copy has to
	// disown it before being saved — the loadVisuals() inside each save call
	// releases whatever the object claims to own, and would otherwise free the
	// model of the object that was just duplicated.
	if (object->type == OBJECT_ENTITY)
	{
		Entity copy = *static_cast<Entity*>(object);
		copy.disownModel();
		copy.rigidBody3D.Teleport(Vector3Add(copy.getPosition(), offset));
		spawned = scene->gameMap.saveEntity(copy);
	}
	else if (object->type == OBJECT_INTERACTABLE)
	{
		InteractableObject copy = *static_cast<InteractableObject*>(object);
		copy.disownModel();
		copy.rigidBody3D.Teleport(Vector3Add(copy.getPosition(), offset));
		spawned = scene->gameMap.saveInteractable(copy);
	}
	else
	{
		GameObject copy = *object;
		copy.disownModel();
		copy.rigidBody3D.Teleport(Vector3Add(copy.getPosition(), offset));
		// saveObject may reallocate gameObjects — `object` is dead after this.
		spawned = scene->gameMap.saveObject(copy);
	}

	if (spawned == nullptr) { statusMessage = "Duplicate failed"; return; }

	spawned->rigidBody3D.SyncCollisionBox();
	selectedObjectId = spawned->id;

	EditorEdit edit = {};
	edit.kind = EditorEdit::EDIT_SPAWN;
	edit.id = spawned->id;
	PushEdit(edit);

	statusMessage = "Duplicated: " + spawned->name;
}

void WorldEditor::FocusOnSelection()
{
	GameObject* object = getSelectedObject();
	if (object == nullptr) { statusMessage = "Nothing selected"; return; }

	// Back off by the object's own size so large geometry frames as well as
	// small, with a floor so a thin object does not put the camera inside it.
	const float radius = fmaxf(Vector3Length(object->getSize()), 1.0f);
	editorCamera.FocusOn(object->getPosition(), radius * 2.5f);
	statusMessage = "Focused: " + object->name;
}

void WorldEditor::SnapSelectionToGrid()
{
	GameObject* object = getSelectedObject();
	if (object == nullptr) { statusMessage = "Nothing selected"; return; }
	if (placementGridStep <= 0.0f) { statusMessage = "Grid step must be positive"; return; }

	GizmoTransform transform = getSelectedTransform(object);

	EditorEdit edit = EditorEdit{ EditorEdit::EDIT_TRANSFORM, object->id,
		transform.position, transform.rotation, transform.scale };

	transform.position = SnapVector(transform.position, placementGridStep);
	ApplyTransform(object, transform);
	PushEdit(edit);

	statusMessage = "Snapped: " + object->name;
}

void WorldEditor::ResetSelectionState()
{
	selectedObjectId = 0;
	gizmo.Cancel();

	// Ids are reassigned by a world load, so every stored id becomes a reference
	// to a different object — or to nothing. Keeping the history would let one
	// Ctrl+Z move an unrelated object.
	undoStack.clear();
	pendingEdit = {};
}

#pragma endregion

#pragma region Edits

void WorldEditor::ApplyTransform(GameObject* object, const GizmoTransform& transform)
{
	if (object == nullptr) { return; }
	auto& body = object->rigidBody3D;

	// RigidBody3D::Update() honors lockTranslation/lockRotation/lockScale, but
	// that only runs while the simulation is live — the editor freezes it while
	// the gizmo/undo path is what's actually writing to the body, so without
	// these checks "Lock Position"/"Lock Rotation" in the Object Browser had no
	// effect while editing: a locked object still moved under a gizmo drag or
	// an Undo.
	// Teleport rather than assigning translation, so lastPosition follows the
	// move instead of pointing at where the object used to be.
	if (!body.lockTranslation) { body.Teleport(transform.position); }
	if (!body.lockRotation) { body.rotation = SafeRotation(transform.rotation); }
	if (!body.lockScale) { body.scale = SanitizeScale(transform.scale); }

	// Any momentum from before the edit would fling the object the instant the
	// simulation resumes, so it would not stay where it was just put.
	body.velocity = {};
	body.acceleration = {};
	body.angularVelocity = {};

	// Mouse picking reads collisionBox, and the frozen simulation is no longer
	// refreshing it. Without this an object can only be re-clicked at the
	// position it occupied before the drag.
	body.SyncCollisionBox();
}

void WorldEditor::PushEdit(const EditorEdit& edit)
{
	undoStack.push_back(edit);
	if (undoStack.size() > UNDO_LIMIT) { undoStack.erase(undoStack.begin()); }
}

void WorldEditor::Undo()
{
	Scene* scene = SceneManager::getInstance().currentScene;
	if (scene == nullptr) { return; }

	// Entries can go stale — the object may have been deleted by hand since.
	// Skipping to the next usable entry beats reporting a failure the user has
	// no way to act on.
	while (!undoStack.empty())
	{
		const EditorEdit edit = undoStack.back();
		undoStack.pop_back();

		if (edit.kind == EditorEdit::EDIT_SPAWN)
		{
			if (!RemoveById(scene, edit.id)) { continue; }
			if (selectedObjectId == edit.id) { selectedObjectId = 0; }
			gizmo.Cancel();
			statusMessage = "Undo: removed spawned object";
			return;
		}

		GameObject* object = FindAnyById(scene, edit.id);
		if (object == nullptr) { continue; }

		ApplyTransform(object, GizmoTransform{ edit.position, edit.rotation, edit.scale });
		selectedObjectId = edit.id;
		gizmo.Cancel();
		statusMessage = "Undo: " + object->name;
		return;
	}

	statusMessage = "Nothing to undo";
}

GizmoTransform WorldEditor::getSelectedTransform(const GameObject* object) const
{
	if (object == nullptr) { return {}; }
	return GizmoTransform{
		object->rigidBody3D.translation,
		SafeRotation(object->rigidBody3D.rotation),
		object->rigidBody3D.scale
	};
}

#pragma endregion

#pragma region Viewport Rendering

/**
 * Selection outline, gizmo and placement ghost.
 *
 * All of it is an overlay: a handle buried inside geometry still has to be
 * grabbable, and the ray test that decides whether it is hovered has no depth
 * buffer to consult. Drawing it depth-tested would let the user hover a handle
 * they cannot see, which reads as the editor ignoring the click.
 *
 * rlgl batches geometry and submits it later, so the batch has to be flushed on
 * both sides of the depth toggle — otherwise the state change lands on the wrong
 * draw calls and hides scene geometry instead.
 */
void WorldEditor::DrawViewport3D()
{
	if (!isEditorActive) { return; }

	// getSelectedObject() walks the scene's containers unguarded, and this runs
	// outside SceneManager_draw's own currentScene check — a scene swap with the
	// editor open would otherwise dereference null here.
	if (SceneManager::getInstance().currentScene == nullptr) { return; }

	const Camera3D& camera = SceneManager::getInstance().camera3D;

	rlDrawRenderBatchActive();
	rlDisableDepthTest();

	if (GameObject* selected = getSelectedObject())
	{
		const GizmoTransform transform = getSelectedTransform(selected);

		DrawOrientedBoxWires(transform.position, Vector3Scale(transform.scale, kOutlineInflate),
			transform.rotation, kSelectionColor);

		// Placement takes over the viewport, so the gizmo stands down rather
		// than competing for the same clicks it can no longer receive.
		if (!placementMode) { gizmo.Draw(camera, transform); }
	}

	if (placementMode && placementPreviewValid)
	{
		Vector3 size = stagingObject.rigidBody3D.scale;
		if (size == Vector3Zero()) { size = Vector3One(); }
		size = SanitizeScale(size);

		DrawCubeV(placementPreviewPosition, size, Fade(kPlacementColor, 0.30f));
		DrawOrientedBoxWires(placementPreviewPosition, size, QuaternionIdentity(), kPlacementColor);

		// Which way the object was pushed off the surface, so a placement onto a
		// wall or ceiling is readable before committing to it.
		DrawLine3D(placementPreviewPosition,
			Vector3Add(placementPreviewPosition, Vector3Scale(placementPreviewNormal, 1.0f)),
			kPlacementColor);
	}

	rlDrawRenderBatchActive();
	rlEnableDepthTest();
}

#pragma endregion
