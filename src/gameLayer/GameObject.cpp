#include "GameObject.h"

#include <asserts.h>
#include <gameMap.h>

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
	
	// Generate 3D Model
	if (meshVariant != MESH_CUSTOM)
	{
		switch (meshVariant)
		{
		case MESH_POLY:	mesh = GenMeshPoly(getSize().x, getSize().y); break;                                            // Generate polygonal mesh
		case MESH_PLANE: mesh = GenMeshPlane(meshData.x, meshData.y, meshData.z, meshData.w); break;                     // Generate plane mesh (with subdivisions)
		case MESH_CUBE: mesh = GenMeshCube(getSize().x, getSize().y, getSize().z); break;                            // Generate cuboid mesh
		case MESH_SPHERE: mesh = GenMeshSphere(getSize().x, getSize().y, getSize().z); break;                              // Generate sphere mesh (standard sphere)
		case MESH_HEMISPHERE: mesh = GenMeshHemiSphere(getSize().x, getSize().y, getSize().z); break;                          // Generate half-sphere mesh (no bottom cap)
		case MESH_CYLINDER: mesh = GenMeshCylinder(getSize().x, getSize().y, getSize().z); break;                         // Generate cylinder mesh
		case MESH_CONE: mesh = GenMeshCone(getSize().x, getSize().y, getSize().z); break;
		}
	}
	model = LoadModelFromMesh(mesh);
	
	// Generate 2d Model

	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(mesh);

	health = getMaxHealth();
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
	}

}


void GameObject::update(GameMap gameMap, float deltaTime)
{
	if (!isEnabled) { return; }
	health = static_cast<int>(Clamp(health, 0, getMaxHealth()));
	// Update Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(mesh);
	rigidBody3D.update(gameMap, deltaTime);
	
	health = Clamp(health, 0, getMaxHealth());

}

