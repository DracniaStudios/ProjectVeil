#pragma once
#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <raylib.h>
#include <Physics.h>


struct Scene;

enum MeshType
{
	MESH_CUSTOM = 0,
	MESH_POLY,
	MESH_PLANE,
	MESH_CUBE,
	MESH_SPHERE,
	MESH_HEMISPHERE,
	MESH_CYLINDER,
	MESH_CONE,
	MESH_COUNT
};

enum ObjectType
{
	OBJECT_GENERIC = 0,
	OBJECT_PLAYER,
	OBJECT_ENTITY,
	OBJECT_ITEM,
	OBJECT_PROJECTILE,
	OBJECT_ENVIRONMENT,
	OBJECT_COUNT
};

struct GameObject 
{
private: 
public:
	//GameObject();
	//GameObject(const GameObject& other);
	//~GameObject();

	/// Data
	const char* name = "GameObject";
	ObjectType type = OBJECT_GENERIC;

	/// Status
	float health = 10.0f;
	bool isAlive = true;
	float lifeSpan = 0;
	float endLife = 0;
	float decayTime = 0;
	void Decay(float time = 0) { decayTime = time; }

	/// Flags
	bool isEnabled = true;
	bool isInteractable = false;
	bool isDestructable = true;
	bool canBeSelected = true;
	
	/// Debug Display
	bool display3DModel = true;
	bool displayDirection = false;
	bool displayCollider = false;

	/// Physics
	RigidBody3D* rigidBody3D = new RigidBody3D{};

	/// Renderer
	Model* model = new Model{};
	Mesh* mesh = new Mesh{};
	Color defaultColor = WHITE;
	Vector4 meshData = Vector4One();
	int meshVariant = MESH_CUBE;
	
	// Developer Menu Texture Test
	int blockID = -1;

	Vector3 getPosition() { return rigidBody3D->translation; }
	Quaternion getRotation() { return rigidBody3D->rotation; }
	Vector3 getSize() { return rigidBody3D->scale; }

	virtual void update(Scene* scene, float deltaTime);
	virtual void render2D();
	virtual void render3D();
	virtual void onEnable();
	virtual void onDisable();
	virtual void onDestroy(Scene* scene);

	virtual void onHit(const GameObject* collider);
	virtual float getMaxHealth() { return 10; }

};

#endif
