#pragma once

#include <raylib.h>   // types only
#include <raymath.h>
#include <cmath>
#include <optional>

// Header-only ports of raylib's non-drawing geometry helpers. Each body is a
// line-for-line copy of the raylib 6.0 function named in its comment, comparison
// operators and float/double types included, so results are bit-identical.
// Nothing in here calls a raylib function; only raylib types and raymath's
// inline math are used.
namespace Geometry {

	// Port of CheckCollisionBoxes, thirdparty/raylib-6.0/src/rmodels.c:4115-4127
	inline bool CheckCollisionBoxes(BoundingBox box1, BoundingBox box2)
	{
		bool collision = true;

		if ((box1.max.x >= box2.min.x) && (box1.min.x <= box2.max.x))
		{
			if ((box1.max.y < box2.min.y) || (box1.min.y > box2.max.y)) collision = false;
			if ((box1.max.z < box2.min.z) || (box1.min.z > box2.max.z)) collision = false;
		}
		else collision = false;

		return collision;
	}

	// Port of CheckCollisionPointRec, rshapes.c:2252-2259
	inline bool CheckCollisionPointRec(Vector2 point, Rectangle rec)
	{
		bool collision = false;

		if ((point.x >= rec.x) && (point.x < (rec.x + rec.width)) && (point.y >= rec.y) && (point.y < (rec.y + rec.height))) collision = true;

		return collision;
	}

	// Port of CheckCollisionRecs, rshapes.c:2313-2321
	inline bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2)
	{
		bool collision = false;

		if ((rec1.x < (rec2.x + rec2.width) && (rec1.x + rec1.width) > rec2.x) &&
			(rec1.y < (rec2.y + rec2.height) && (rec1.y + rec1.height) > rec2.y)) collision = true;

		return collision;
	}

	// Port of CheckCollisionCircleRec, rshapes.c:2341-2365
	inline bool CheckCollisionCircleRec(Vector2 center, float radius, Rectangle rec)
	{
		bool collision = false;

		float recCenterX = rec.x + rec.width/2.0f;
		float recCenterY = rec.y + rec.height/2.0f;

		float dx = fabsf(center.x - recCenterX);
		float dy = fabsf(center.y - recCenterY);

		if ((dx <= (rec.width/2.0f + radius)) && (dy <= (rec.height/2.0f + radius)))
		{
			if (dx <= (rec.width/2.0f)) collision = true;
			else if (dy <= (rec.height/2.0f)) collision = true;
			else
			{
				float cornerDistanceSq = (dx - rec.width/2.0f)*(dx - rec.width/2.0f) +
					(dy - rec.height/2.0f)*(dy - rec.height/2.0f);

				collision = (cornerDistanceSq <= (radius*radius));
			}
		}

		return collision;
	}

	// Port of GetMeshBoundingBox, rmodels.c:3715-3740. vertices holds xyz triples;
	// a {0,0,0}-{0,0,0} box when vertices is null.
	inline BoundingBox GetMeshBoundingBox(const float* vertices, int vertexCount)
	{
		// Get min and max vertex to construct bounds (AABB)
		Vector3 minVertex = { 0 };
		Vector3 maxVertex = { 0 };

		if (vertices != NULL)
		{
			minVertex = Vector3{ vertices[0], vertices[1], vertices[2] };
			maxVertex = Vector3{ vertices[0], vertices[1], vertices[2] };

			for (int i = 1; i < vertexCount; i++)
			{
				minVertex = Vector3Min(minVertex, Vector3{ vertices[i*3], vertices[i*3 + 1], vertices[i*3 + 2] });
				maxVertex = Vector3Max(maxVertex, Vector3{ vertices[i*3], vertices[i*3 + 1], vertices[i*3 + 2] });
			}
		}

		// Create the bounding box
		BoundingBox box = { 0 };
		box.min = minVertex;
		box.max = maxVertex;

		return box;
	}

	// Port of GetScreenToWorldRayEx, rcore.c:1426-1478. raylib's rlGetCullDistanceNear/Far
	// are replaced by their defaults, the doubles 0.05 and 4000.0 (nothing calls rlSetClipPlanes).
	inline Ray GetScreenToWorldRayEx(Vector2 position, Camera3D camera, int width, int height)
	{
		Ray ray = { 0 };

		// Calculate normalized device coordinates
		// NOTE: y value is negative
		float x = (2.0f*position.x)/(float)width - 1.0f;
		float y = 1.0f - (2.0f*position.y)/(float)height;
		float z = 1.0f;

		// Store values in a vector
		Vector3 deviceCoords = { x, y, z };

		// Calculate view matrix from camera look at
		Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

		Matrix matProj = MatrixIdentity();

		if (camera.projection == CAMERA_PERSPECTIVE)
		{
			// Calculate projection matrix from perspective
			matProj = MatrixPerspective(camera.fovy*DEG2RAD, ((double)width/(double)height), 0.05, 4000.0);
		}
		else if (camera.projection == CAMERA_ORTHOGRAPHIC)
		{
			double aspect = (double)width/(double)height;
			double top = camera.fovy/2.0;
			double right = top*aspect;

			// Calculate projection matrix from orthographic
			matProj = MatrixOrtho(-right, right, -top, top, 0.05, 4000.0);
		}

		// Unproject far/near points
		Vector3 nearPoint = Vector3Unproject(Vector3{ deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
		Vector3 farPoint = Vector3Unproject(Vector3{ deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

		// Unproject the mouse cursor in the near plane
		// It is needed as the source position because orthographic projects,
		// compared to perspective doesn't have a convergence point,
		// meaning that the "eye" of the camera is more like a plane than a point
		Vector3 cameraPlanePointerPos = Vector3Unproject(Vector3{ deviceCoords.x, deviceCoords.y, -1.0f }, matProj, matView);

		// Calculate normalized direction vector
		Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

		if (camera.projection == CAMERA_PERSPECTIVE) ray.position = camera.position;
		else if (camera.projection == CAMERA_ORTHOGRAPHIC) ray.position = cameraPlanePointerPos;

		// Apply calculated vectors to ray
		ray.direction = direction;

		return ray;
	}

	// Port of Fade, rtextures.c:4841-4851
	inline Color Fade(Color color, float alpha)
	{
		Color result = color;

		if (alpha < 0.0f) alpha = 0.0f;
		else if (alpha > 1.0f) alpha = 1.0f;

		result.a = (unsigned char)(255.0f*alpha);

		return result;
	}

	// Grows bounds by add; an empty optional becomes add. Not a raylib port.
	inline void ExtendBounds(std::optional<BoundingBox>& bounds, BoundingBox add)
	{
		if (!bounds.has_value())
		{
			bounds = add;
			return;
		}
		bounds->min = Vector3Min(bounds->min, add.min);
		bounds->max = Vector3Max(bounds->max, add.max);
	}

}
