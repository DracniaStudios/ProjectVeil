#include "GameObject.h"

BoundingBox getBoundingBox(Model mdl, Vector3 pos)
{
	BoundingBox box = GetMeshBoundingBox(mdl.meshes[0]);
	box.min = Vector3Add(pos, box.min);
	box.max = Vector3Add(pos, box.max);
	return box;
}

void GameObject::onEnable()
{
	isEnabled = true;
	mesh = GenMeshCube(rigidBody3D.scale.x, rigidBody3D.scale.y, rigidBody3D.scale.z);
	model = LoadModelFromMesh(mesh);
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
	DrawRectangle(rigidBody3D.translation.x - rigidBody3D.scale.x / 2,
		rigidBody3D.translation.y - rigidBody3D.scale.y / 2,
		rigidBody3D.scale.x,
		rigidBody3D.scale.y,
		GREEN);
}
void GameObject::render3D()
{
	DrawSphere(rigidBody3D.translation, 0.1f, BLACK);
	if (!isEnabled) { return; }
	DrawBoundingBox(rigidBody3D.collisionBox, WHITE);
	DrawModel(model, rigidBody3D.translation, 1.0f, Color{20, 30, 30, 255});
	DrawModelWires(model, rigidBody3D.translation, 1.0f, BLACK);
}


void GameObject::update(float deltaTime)
{
	if (!isEnabled) { return; }
	// Update Data
	rigidBody3D.update(deltaTime);
	rigidBody3D.collisionBox = getBoundingBox(model, rigidBody3D.translation);

}

