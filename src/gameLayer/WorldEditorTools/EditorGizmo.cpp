#include "EditorGizmo.h"

#include "EditorPicking.h"

#include <cfloat>
#include <cmath>

namespace
{
	// Handle proportions, all relative to HandleLength() so the gizmo keeps a
	// constant apparent size no matter how far away the pivot is.
	constexpr float kAxisGrabTolerance = 0.14f;
	constexpr float kRingGrabTolerance = 0.12f;
	constexpr float kUniformGrabRadius = 0.18f;
	constexpr float kArrowHeadLength = 0.25f;
	constexpr float kArrowHeadRadius = 0.07f;
	constexpr float kScaleBlockSize = 0.12f;
	constexpr float kUniformBlockSize = 0.14f;

	constexpr int kRingSegments = 48;

	// A ring seen closer to edge-on than this has no usable plane to project the
	// cursor onto — the projected radius explodes and the angle is noise.
	constexpr float kMinimumRingFacing = 0.08f;

	const Color kAxisColors[3] = {
		Color{ 220,  60,  60, 255 }, // X
		Color{  70, 210,  90, 255 }, // Y
		Color{  70, 130, 235, 255 }, // Z
	};
	const Color kHighlightColor = Color{ 255, 220,  60, 255 };

	Vector3 UnitAxis(int axis)
	{
		return {
			axis == 0 ? 1.0f : 0.0f,
			axis == 1 ? 1.0f : 0.0f,
			axis == 2 ? 1.0f : 0.0f
		};
	}

	float& Component(Vector3& vector, int axis)
	{
		return axis == 0 ? vector.x : (axis == 1 ? vector.y : vector.z);
	}

	/**
	 * Two orthonormal vectors spanning the plane with the given normal.
	 *
	 * The reference axis is swapped near the poles because a cross product with
	 * a near-parallel vector normalises to noise.
	 */
	void BuildPlaneBasis(Vector3 normal, Vector3& outU, Vector3& outV)
	{
		const Vector3 reference = (fabsf(normal.y) > 0.99f)
			? Vector3{ 1.0f, 0.0f, 0.0f }
			: Vector3{ 0.0f, 1.0f, 0.0f };

		outU = Vector3Normalize(Vector3CrossProduct(reference, normal));
		outV = Vector3CrossProduct(normal, outU);
	}

	/** The camera's right vector, used as the drag rail for uniform scaling. */
	Vector3 CameraRight(const Camera3D& camera)
	{
		const Vector3 forward = Vector3Subtract(camera.target, camera.position);
		if (Vector3LengthSqr(forward) < 1.0e-6f) { return Vector3{ 1.0f, 0.0f, 0.0f }; }

		const Vector3 right = Vector3CrossProduct(Vector3Normalize(forward), camera.up);
		if (Vector3LengthSqr(right) < 1.0e-6f) { return Vector3{ 1.0f, 0.0f, 0.0f }; }
		return Vector3Normalize(right);
	}

	/** Line plus a cone at the tip. */
	void DrawArrow(Vector3 origin, Vector3 direction, float length, Color color)
	{
		const Vector3 tip = Vector3Add(origin, Vector3Scale(direction, length));
		const Vector3 neck = Vector3Add(origin, Vector3Scale(direction, length * (1.0f - kArrowHeadLength)));

		DrawLine3D(origin, neck, color);
		DrawCylinderEx(neck, tip, length * kArrowHeadRadius, 0.0f, 10, color);
	}

	/**
	 * Ring drawn from an explicit in-plane basis.
	 *
	 * raylib's DrawCircle3D takes an axis/angle pair relative to the XY plane,
	 * which needs a rotation derived per ring and breaks down when the target
	 * axis is antiparallel to Z. Emitting the segments directly removes both
	 * the conversion and the degenerate case.
	 */
	void DrawRing3D(Vector3 center, Vector3 normal, float radius, Color color)
	{
		Vector3 u = {};
		Vector3 v = {};
		BuildPlaneBasis(Vector3Normalize(normal), u, v);

		Vector3 previous = Vector3Add(center, Vector3Scale(u, radius));
		for (int segment = 1; segment <= kRingSegments; ++segment)
		{
			const float angle = (2.0f * PI * static_cast<float>(segment)) / static_cast<float>(kRingSegments);
			const Vector3 point = Vector3Add(center,
				Vector3Add(Vector3Scale(u, cosf(angle) * radius), Vector3Scale(v, sinf(angle) * radius)));

			DrawLine3D(previous, point, color);
			previous = point;
		}
	}
}

Color GizmoAxisColor(int axis, bool highlighted)
{
	if (highlighted) { return kHighlightColor; }
	if (axis < 0 || axis > 2) { return WHITE; }
	return kAxisColors[axis];
}

float EditorGizmo::HandleLength(const Camera3D& camera, Vector3 center) const
{
	// Constant apparent size. Without this the gizmo is an unclickable dot at
	// range and swallows the screen up close. Floored so a camera sitting on top
	// of the pivot cannot collapse it to zero, which would make every grab
	// tolerance zero as well and leave the gizmo permanently unhittable.
	return fmaxf(Vector3Distance(camera.position, center) * 0.14f, 0.08f);
}

Vector3 EditorGizmo::AxisDirection(int axis, const GizmoTransform& transform) const
{
	// Scale is always local. A world-aligned scale handle on a rotated object
	// would write into a scale component that no longer points where the arrow
	// does — the object would stretch along an axis the user did not grab.
	const bool useLocal = localSpace || mode == GIZMO_SCALE;
	if (!useLocal) { return UnitAxis(axis); }

	return Vector3Normalize(Vector3RotateByQuaternion(UnitAxis(axis), SafeRotation(transform.rotation)));
}

void EditorGizmo::Cancel()
{
	activeHandle = GIZMO_HANDLE_NONE;
	hoveredHandle = GIZMO_HANDLE_NONE;
	dragBegan = false;
	dragEnded = false;
}

bool EditorGizmo::Update(const Camera3D& camera, GizmoTransform& transform, bool canStartDrag)
{
	// Edges are single-frame signals; clear them before anything can set them.
	dragBegan = false;
	dragEnded = false;

	if (mode == GIZMO_SELECT)
	{
		Cancel();
		return false;
	}

	const Ray ray = GetEditorMouseRay(camera);

	if (activeHandle == GIZMO_HANDLE_NONE)
	{
		const float length = HandleLength(camera, transform.position);
		hoveredHandle = PickHandle(ray, transform, length);

		if (canStartDrag && hoveredHandle != GIZMO_HANDLE_NONE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		{
			if (BeginDrag(camera, ray, transform, hoveredHandle)) { dragBegan = true; }
		}

		// A hovered handle consumes the click even if the grab could not start
		// (a degenerate axis, say), so a missed grab never silently deselects
		// the object the user was aiming at.
		return hoveredHandle != GIZMO_HANDLE_NONE;
	}

	// Escape aborts and restores the transform the drag started from. Checked
	// before the mouse state so an abort is never mistaken for a commit.
	if (IsKeyPressed(KEY_ESCAPE))
	{
		transform = startTransform;
		Cancel();
		return true;
	}

	// The drag owns the mouse until the button comes up, wherever the cursor is.
	if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{
		activeHandle = GIZMO_HANDLE_NONE;
		dragEnded = true;
		return true;
	}

	ApplyDrag(ray, transform);
	return true;
}

GizmoHandle EditorGizmo::PickHandle(const Ray& ray, const GizmoTransform& transform, float length) const
{
	if (mode == GIZMO_ROTATE)
	{
		GizmoHandle best = GIZMO_HANDLE_NONE;
		float bestError = length * kRingGrabTolerance;

		for (int axis = 0; axis < 3; ++axis)
		{
			const Vector3 normal = AxisDirection(axis, transform);
			if (fabsf(Vector3DotProduct(Vector3Normalize(ray.direction), normal)) < kMinimumRingFacing)
			{
				continue; // edge-on, and therefore not grabbable
			}

			Vector3 point = {};
			if (!RayPlane(ray, transform.position, normal, point)) { continue; }

			const float error = fabsf(Vector3Distance(point, transform.position) - length);
			if (error < bestError) { bestError = error; best = static_cast<GizmoHandle>(axis); }
		}
		return best;
	}

	// The uniform block sits on top of where all three bars meet, so it has to
	// be tested first or it would be unreachable.
	if (mode == GIZMO_SCALE && RayPointDistance(ray, transform.position) < length * kUniformGrabRadius)
	{
		return GIZMO_HANDLE_UNIFORM;
	}

	GizmoHandle best = GIZMO_HANDLE_NONE;
	float bestDistance = length * kAxisGrabTolerance;

	for (int axis = 0; axis < 3; ++axis)
	{
		const float distance = RayAxisDistance(ray, transform.position, AxisDirection(axis, transform), length);
		if (distance < bestDistance) { bestDistance = distance; best = static_cast<GizmoHandle>(axis); }
	}
	return best;
}

bool EditorGizmo::BeginDrag(const Camera3D& camera, const Ray& ray,
	const GizmoTransform& transform, GizmoHandle handle)
{
	startTransform = transform;

	if (mode == GIZMO_ROTATE)
	{
		dragAxis = AxisDirection(handle, transform);

		// The basis is frozen for the whole drag. Recomputing it per frame would
		// be fine in world space, but in local space the axis turns with the
		// object — the reference direction would chase the rotation and the
		// object would spin away on its own.
		BuildPlaneBasis(dragAxis, dragBasisU, dragBasisV);

		float angle = 0.0f;
		if (!PlaneAngle(ray, transform.position, angle)) { return false; }

		dragAngle = 0.0f;
		dragPreviousAngle = angle;
	}
	else
	{
		// Uniform scale has no axis of its own, so it borrows the camera's right
		// vector — a rail that always has a stable screen direction.
		dragAxis = (handle == GIZMO_HANDLE_UNIFORM)
			? CameraRight(camera)
			: AxisDirection(handle, transform);

		if (!ClosestPointOnAxis(ray, transform.position, dragAxis, dragStartT)) { return false; }
	}

	activeHandle = handle;
	return true;
}

bool EditorGizmo::PlaneAngle(const Ray& ray, Vector3 center, float& outAngle) const
{
	if (fabsf(Vector3DotProduct(Vector3Normalize(ray.direction), dragAxis)) < kMinimumRingFacing)
	{
		return false;
	}

	Vector3 point = {};
	if (!RayPlane(ray, center, dragAxis, point)) { return false; }

	const Vector3 offset = Vector3Subtract(point, center);
	if (Vector3LengthSqr(offset) < 1.0e-6f) { return false; } // cursor on the pivot: angle undefined

	outAngle = atan2f(Vector3DotProduct(offset, dragBasisV), Vector3DotProduct(offset, dragBasisU));
	return true;
}

void EditorGizmo::ApplyDrag(const Ray& ray, GizmoTransform& transform)
{
	switch (mode)
	{
	case GIZMO_TRANSLATE:
	{
		float t = 0.0f;
		if (!ClosestPointOnAxis(ray, startTransform.position, dragAxis, t)) { return; }

		// Always measured against the grab, never accumulated frame to frame:
		// an incremental delta drifts, and any frame the solve is refused would
		// permanently offset the object from the cursor.
		float delta = t - dragStartT;
		if (snapEnabled) { delta = SnapValue(delta, translateSnap); }

		transform.position = Vector3Add(startTransform.position, Vector3Scale(dragAxis, delta));
		break;
	}

	case GIZMO_SCALE:
	{
		float t = 0.0f;
		if (!ClosestPointOnAxis(ray, startTransform.position, dragAxis, t)) { return; }

		float delta = t - dragStartT;
		if (snapEnabled) { delta = SnapValue(delta, scaleSnap); }

		// Additive, not a ratio. A multiplicative t / dragStartT blows up when
		// the grab lands near the pivot, where dragStartT is ~0.
		Vector3 scale = startTransform.scale;
		if (activeHandle == GIZMO_HANDLE_UNIFORM) { scale = Vector3AddValue(scale, delta); }
		else { Component(scale, activeHandle) += delta; }

		transform.scale = SanitizeScale(scale);
		break;
	}

	case GIZMO_ROTATE:
	{
		float angle = 0.0f;
		if (!PlaneAngle(ray, startTransform.position, angle)) { return; }

		// Accumulate the shortest step each frame rather than differencing
		// against the grab angle: atan2 wraps at +/-pi, and a raw difference
		// would snap the object a full turn backwards every time the cursor
		// crossed that seam. Accumulating also allows turns beyond 180 degrees.
		float step = angle - dragPreviousAngle;
		while (step > PI) { step -= 2.0f * PI; }
		while (step < -PI) { step += 2.0f * PI; }

		dragPreviousAngle = angle;
		dragAngle += step;

		float applied = dragAngle;
		if (snapEnabled) { applied = SnapValue(applied, rotateSnapDegrees * DEG2RAD); }

		// Pre-multiplying by a rotation about a world-space direction turns the
		// object about that direction through its own centre, which is what both
		// world and local mode want — the local axis is already expressed in
		// world space by AxisDirection().
		transform.rotation = QuaternionNormalize(QuaternionMultiply(
			QuaternionFromAxisAngle(dragAxis, applied), SafeRotation(startTransform.rotation)));
		break;
	}

	default:
		break;
	}
}

void EditorGizmo::Draw(const Camera3D& camera, const GizmoTransform& transform) const
{
	if (mode == GIZMO_SELECT) { return; }

	const float length = HandleLength(camera, transform.position);
	const GizmoHandle highlight = (activeHandle != GIZMO_HANDLE_NONE) ? activeHandle : hoveredHandle;

	for (int axis = 0; axis < 3; ++axis)
	{
		const Vector3 direction = AxisDirection(axis, transform);
		const Color color = GizmoAxisColor(axis, highlight == axis);

		switch (mode)
		{
		case GIZMO_TRANSLATE:
			DrawArrow(transform.position, direction, length, color);
			break;

		case GIZMO_ROTATE:
			DrawRing3D(transform.position, direction, length, color);
			break;

		case GIZMO_SCALE:
		{
			const Vector3 tip = Vector3Add(transform.position, Vector3Scale(direction, length));
			DrawLine3D(transform.position, tip, color);
			DrawCubeV(tip, Vector3Scale(Vector3One(), length * kScaleBlockSize), color);
			break;
		}

		default:
			break;
		}
	}

	if (mode == GIZMO_SCALE)
	{
		DrawCubeV(transform.position, Vector3Scale(Vector3One(), length * kUniformBlockSize),
			GizmoAxisColor(-1, highlight == GIZMO_HANDLE_UNIFORM));
	}
}
