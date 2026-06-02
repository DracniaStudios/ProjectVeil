#include "GameObject.h"

#include <asserts.h>
#include <gameMap.h>

#include <AssetManager.h>
#include <Scene.h>

BoundingBox getBoundingBox(Model mdl, Vector3 pos)
{
	permaAssertComment(mdl.meshes == nullptr, "No Meshes In Model");
	
	BoundingBox box = GetMeshBoundingBox(mdl.meshes[0]);
	box.min = Vector3Add(pos, box.min);
	box.max = Vector3Add(pos, box.max);
	return box;
}

/** Initialization **/
/*
GameObject::GameObject()
{
	isEnabled = true;

	// Set Initial Data
	rigidBody3D->owner = this;
	rigidBody3D->collisionBox = GetMeshBoundingBox(*mesh);
}

GameObject::GameObject(const GameObject& other)
{
	isEnabled = true;

	rigidBody3D = new RigidBody3D(*other.rigidBody3D);
	rigidBody3D->owner = this;
	model = other.model ? new Model(*other.model) : nullptr;
	mesh = other.mesh ? new Mesh(*other.mesh) : nullptr;
	rigidBody3D->collisionBox = GetMeshBoundingBox(*mesh);
}
GameObject::~GameObject()
{
	delete rigidBody3D;
	//UnloadModel(*model);
	delete model;
	//UnloadMesh(*mesh);
	delete mesh;
}
*/

/** Lifecycle **/

void GameObject::onEnable()
{
	isEnabled = true;

	if (meshVariant != MESH_CUSTOM)
	{
		switch (meshVariant)
		{
		case MESH_POLY:	*mesh = GenMeshPoly(getSize().x, getSize().y); break;											// Generate polygonal mesh
		case MESH_PLANE: *mesh = GenMeshPlane(meshData.x, meshData.y, meshData.z, meshData.w); break;                     // Generate plane mesh (with subdivisions)
		case MESH_CUBE: *mesh = GenMeshCube(getSize().x, getSize().y, getSize().z); break;                            // Generate cuboid mesh
		case MESH_SPHERE: *mesh = GenMeshSphere(getSize().x, getSize().y, getSize().z); break;                              // Generate sphere mesh (standard sphere)
		case MESH_HEMISPHERE: *mesh = GenMeshHemiSphere(getSize().x, getSize().y, getSize().z); break;                          // Generate half-sphere mesh (no bottom cap)
		case MESH_CYLINDER: *mesh = GenMeshCylinder(getSize().x, getSize().y, getSize().z); break;                         // Generate cylinder mesh
		case MESH_CONE: *mesh = GenMeshCone(getSize().x, getSize().y, getSize().z); break;
		}
	}
	*model = LoadModelFromMesh(*mesh);
	
	if (blockID >= 0)
	{
		//SetMaterialTexture(&model.materials[0], MATERIAL_MAP_ALBEDO, getTextureFromID(blockID));
		model->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = getTextureFromID(blockID);
		defaultColor = WHITE;
	}
	
	// Set Initial Data
	rigidBody3D->owner = this;
	rigidBody3D->collisionBox = GetMeshBoundingBox(*mesh);

	lifeSpan = 0;
	endLife = 1;
}

void GameObject::onDisable()
{
	isEnabled = false;
	UnloadMesh(*mesh);
	UnloadModel(*model);
}

// Destroy Object After Delay (in milliseconds)
void GameObject::onDestroy(Scene* scene)
{
	scene->gameMap.removeObject(this);
}

void GameObject::render2D()
{
	if (!isEnabled) { return; }
}
void GameObject::render3D()
{
	if (!isEnabled) { return; }

	if (displayCollider) { DrawBoundingBox(rigidBody3D->collisionBox, WHITE); }

	if (display3DModel) {
		DrawModel(*model, rigidBody3D->translation, 1.0f, defaultColor);
		DrawModelWires(*model, rigidBody3D->translation, 1.0f, BLACK);
	}

	if (displayDirection) {
		/// Show Directions
		DrawSphere(rigidBody3D->forward + rigidBody3D->translation, 0.1f, RED);
		DrawSphere(rigidBody3D->back + rigidBody3D->translation, 0.1f, ORANGE);
		DrawSphere(rigidBody3D->left + rigidBody3D->translation, 0.1f, YELLOW);
		DrawSphere(rigidBody3D->right + rigidBody3D->translation, 0.1f, GREEN);
		DrawSphere(rigidBody3D->up + rigidBody3D->translation, 0.1f, BLUE);
		DrawSphere(rigidBody3D->down + rigidBody3D->translation, 0.1f, PURPLE);
	}

	auto displayRay = [&](Ray ray, Color color)
	{
		DrawLine3D(ray.position, Vector3Add(ray.position, Vector3Scale(ray.direction, 0.5f)), color);
	};
}

void setLife(float& life, int time) { life = static_cast<float>(GetTime() + time); }

void GameObject::update(Scene* scene, float deltaTime)
{
	if (!isEnabled) { return; }

	// Update Data
	rigidBody3D->collisionBox = GetMeshBoundingBox(*mesh);
	rigidBody3D->update(scene->gameMap, deltaTime);

	if (isAlive) { endLife += deltaTime; }
	lifeSpan += deltaTime;

	if (isDestructable) {
		if (health <= 0) {
			if (isAlive) { setLife(endLife, decayTime); }
			isAlive = false;
		}
		if (lifeSpan > endLife) {
			onDestroy(scene);
		}
	}
}

void GameObject::onHit(const GameObject* collider)
{
	std::cout << name << " was hit by " << collider->name << "!\n";
}

// Remove Object From Scene and Destroy It.
// Called When Object's Health Reaches 0, or When Object is Otherwise Destroyed.
