#pragma once
#ifndef EDITOR_GIZMO_H
#define EDITOR_GIZMO_H

#include <raylib.h>
#include <raymath.h>

#include "EditorPicking.h"

/** Which manipulator the viewport is currently running. */
enum GizmoMode
{
	GIZMO_SELECT,    // Click to select; no handles drawn and no drag possible
	GIZMO_TRANSLATE,
	GIZMO_ROTATE,
	GIZMO_SCALE,
	GIZMO_MODE_COUNT
};

/** Which part of the gizmo the mouse is over or has grabbed. */
enum GizmoHandle
{
	GIZMO_HANDLE_NONE = -1,
	GIZMO_HANDLE_X = 0,
	GIZMO_HANDLE_Y = 1,
	GIZMO_HANDLE_Z = 2,
	GIZMO_HANDLE_UNIFORM = 3, // Scale mode only — the centre block
};

/**
 * The transform the gizmo reads and writes.
 *
 * Deliberately not a GameObject pointer: a pointer only survives until its
 * own object is destroyed, and the gizmo can outlive that. The editor
 * resolves its selection by id once per frame and hands the gizmo a plain
 * value type valid for the duration of that frame only.
 */
struct GizmoTransform
{
	Vector3 position = {};
	Quaternion rotation = QuaternionIdentity();
	Vector3 scale = Vector3One();
};

/**
 * Screen-space-correct translate / rotate / scale manipulator.
 *
 * Owns nothing in the scene: it is handed a transform, mutates it in place, and
 * reports whether it consumed the mouse this frame. The caller is responsible
 * for writing the result back onto a rigid body (and for refreshing that body's
 * collision box, which the frozen simulation is no longer doing).
 */
class EditorGizmo
{
public:
	/** Tool state, driven directly by the World Editor hub. */
	GizmoMode mode = GIZMO_TRANSLATE;
	bool localSpace = false;   // Align handles to the object instead of the world
	bool snapEnabled = false;
	float translateSnap = 0.5f;
	float rotateSnapDegrees = 15.0f;
	float scaleSnap = 0.25f;

	// Scaling clamps to this; see MINIMUM_EDITOR_SCALE for why it exists.
	static constexpr float kMinimumScale = MINIMUM_EDITOR_SCALE;

	/**
	 * Runs hover detection and any in-flight drag for one frame.
	 *
	 * @param canStartDrag  Whether a *new* grab may begin. False while ImGui owns
	 *                      the mouse or the camera is looking around. An already
	 *                      running drag deliberately ignores this, so sweeping
	 *                      the cursor over a panel mid-move does not drop the
	 *                      object halfway.
	 * @return true when the gizmo owns the mouse and the caller must not also
	 *         treat the click as a selection.
	 */
	bool Update(const Camera3D& camera, GizmoTransform& transform, bool canStartDrag);

	/** Draws the handles. Expects to be inside BeginMode3D with depth off. */
	void Draw(const Camera3D& camera, const GizmoTransform& transform) const;

	/** Drops any grab. Must be called before the dragged object is destroyed. */
	void Cancel();

	bool IsDragging() const { return activeHandle != GIZMO_HANDLE_NONE; }
	bool IsHovered() const { return hoveredHandle != GIZMO_HANDLE_NONE; }

	/** Single-frame edges, for pushing exactly one undo entry per drag. */
	bool DragBegan() const { return dragBegan; }
	bool DragEnded() const { return dragEnded; }
	const GizmoTransform& DragStartTransform() const { return startTransform; }

	/** Apparent-size-constant handle length at a given pivot. */
	float HandleLength(const Camera3D& camera, Vector3 center) const;

private:
	GizmoHandle hoveredHandle = GIZMO_HANDLE_NONE;
	GizmoHandle activeHandle = GIZMO_HANDLE_NONE;
	bool dragBegan = false;
	bool dragEnded = false;

	/** Everything below is captured once at grab time and held for the drag. */
	GizmoTransform startTransform = {};
	Vector3 dragAxis = {};           // World-space direction of the grabbed handle
	float dragStartT = 0.0f;         // Axis parameter under the cursor at grab time
	float dragAngle = 0.0f;          // Accumulated rotation, radians
	float dragPreviousAngle = 0.0f;  // Last raw atan2 reading, for unwrapping
	Vector3 dragBasisU = {};         // Frozen in-plane basis for the rotate ring
	Vector3 dragBasisV = {};

	Vector3 AxisDirection(int axis, const GizmoTransform& transform) const;
	GizmoHandle PickHandle(const Ray& ray, const GizmoTransform& transform, float length) const;
	bool BeginDrag(const Camera3D& camera, const Ray& ray, const GizmoTransform& transform, GizmoHandle handle);
	void ApplyDrag(const Ray& ray, GizmoTransform& transform);
	bool PlaneAngle(const Ray& ray, Vector3 center, float& outAngle) const;
};

/** Colour of a handle, accounting for hover/active highlighting. */
Color GizmoAxisColor(int axis, bool highlighted);

// DrawOrientedBoxWires — used here for the selection outline and the placement
// ghost — now lives in Physics.h alongside the collider debug draw that shares it.

#endif
