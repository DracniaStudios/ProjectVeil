#include "GameObject.h"

#include <asserts.h>

BoundingBox getBoundingBox(Model mdl, Vector3 pos)
{
	//permaAssertComment(mdl.meshes == nullptr, "No Meshes In Model");
	BoundingBox box = GetMeshBoundingBox(mdl.meshes[0]);
	box.min = Vector3Add(pos, box.min);
	box.max = Vector3Add(pos, box.max);
	return box;
}

void GameObject::onEnable()
{
	isEnabled = true;
	//rigidBody2D.scale = Vector3(1, 1, 1);
	rigidBody3D.scale = Vector3(1, 1, 1);
	
	// Generate 3D Model
	mesh = GenMeshCube(rigidBody3D.scale.x, rigidBody3D.scale.y, rigidBody3D.scale.z);
	model = LoadModelFromMesh(mesh);
	
	// Generate 2d Model

	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(mesh);
}

void GameObject::onDisable()
{
	isEnabled = false;
	UnloadModel(model);
	UnloadMesh(mesh);
}

void GameObject::render2D()
{
	if (!isEnabled) { return; }
	/*
	Rectangle pos2D = { rigidBody2D.translation.x - rigidBody2D.scale.x / 2,
		rigidBody2D.translation.y - rigidBody2D.scale.y / 2,
		rigidBody3D.scale.x * 32,
		rigidBody3D.scale.y * 32 };

	pos2D.y -= rigidBody3D.scale.y * 32;

	if (display2DModel) {
		DrawRectangle(pos2D.x, pos2D.y, pos2D.width, pos2D.height, defaultColor);
	}
	*/
}
void GameObject::render3D()
{
	if (!isEnabled) { return; }

	if (displayCollider) { DrawBoundingBox(rigidBody3D.collisionBox, WHITE); }

	if (display3DModel) {
		DrawModel(model, rigidBody3D.translation, 1.0f, defaultColor);
		DrawModelWires(model, rigidBody3D.translation, 1.0f, BLACK);
	}
	if (displayDirection) {
		/// Show Directions
		DrawSphere(rigidBody3D.front + rigidBody3D.translation, 0.1f, RED);
		DrawSphere(rigidBody3D.back + rigidBody3D.translation, 0.1f, ORANGE);
		DrawSphere(rigidBody3D.left + rigidBody3D.translation, 0.1f, YELLOW);
		DrawSphere(rigidBody3D.right + rigidBody3D.translation, 0.1f, GREEN);
		DrawSphere(rigidBody3D.up + rigidBody3D.translation, 0.1f, BLUE);
		DrawSphere(rigidBody3D.down + rigidBody3D.translation, 0.1f, PURPLE);

		/// Show Ray Lines
		DrawRay(Ray{ rigidBody3D.translation, rigidBody3D.front }, RED);
		DrawRay(Ray{ rigidBody3D.translation, rigidBody3D.back }, ORANGE);
		DrawRay(Ray{ rigidBody3D.translation, rigidBody3D.left }, YELLOW);
		DrawRay(Ray{ rigidBody3D.translation, rigidBody3D.right }, GREEN);
		DrawRay(Ray{ rigidBody3D.translation, rigidBody3D.up }, BLUE);
		DrawRay(Ray{ rigidBody3D.translation, rigidBody3D.down }, PURPLE);
	}

}


void GameObject::update(float deltaTime)
{
	if (!isEnabled) { return; }
	// Update Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(this->mesh);
	rigidBody3D.update(deltaTime);
	/*
	// Clamp 2D to position from game map size to screen size
	{
		const float screenX = static_cast<float>(GetScreenWidth());
		const float screenY = static_cast<float>(GetScreenHeight());

		rigidBody3D.translation.x = Clamp(rigidBody3D.translation.x, -(screenX / 10) + rigidBody3D.scale.x / 2, screenX / 10 - rigidBody3D.scale.x / 2);
		rigidBody3D.translation.y = Clamp(rigidBody3D.translation.y, 0, screenY / 2 - rigidBody3D.scale.y / 2);
		rigidBody3D.translation.z = Clamp(rigidBody3D.translation.z, -(screenX / 10) + rigidBody3D.scale.z / 2, screenX / 10 - rigidBody3D.scale.z / 2);

		rigidBody2D.translation.x = screenX / 2 + rigidBody3D.translation.x * 10;
		rigidBody2D.translation.y = screenY - rigidBody3D.translation.y * 10;
		// Clamp object within bounds by calculating 108 - scale.x / 2

		rigidBody2D.translation.x = Clamp(rigidBody2D.translation.x, 0, screenX);
		rigidBody2D.translation.y = Clamp(rigidBody2D.translation.y, 0, screenY);

	}
	*/
}

