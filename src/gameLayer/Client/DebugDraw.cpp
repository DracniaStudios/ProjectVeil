#include "DebugDraw.h"

#include <raymath.h>

#include <Render/Render.h>

#include <Collider.h>

	void DrawOrientedBoxWires(Vector3 center, Vector3 size, Quaternion rotation, Color color)
	{
		const Quaternion orientation = SafeOrientation(rotation);
		const Vector3 half = Vector3Scale(size, 0.5f);

		// Eight corners of the local box, rotated into world space. Each index bit
		// selects the sign of one axis.
		Vector3 corner[8] = {};
		for (int index = 0; index < 8; ++index)
		{
			const Vector3 local = {
				(index & 1) ? half.x : -half.x,
				(index & 2) ? half.y : -half.y,
				(index & 4) ? half.z : -half.z
			};
			corner[index] = Vector3Add(center, Vector3RotateByQuaternion(local, orientation));
		}

		// Index pairs differing by exactly one bit are exactly the 12 edges
		static constexpr int edges[12][2] = {
			{ 0,1 },{ 2,3 },{ 4,5 },{ 6,7 },
			{ 0,2 },{ 1,3 },{ 4,6 },{ 5,7 },
			{ 0,4 },{ 1,5 },{ 2,6 },{ 3,7 }
		};

		for (const auto& edge : edges)
		{
			Render::DebugLine(corner[edge[0]], corner[edge[1]], color);
		}
	}
