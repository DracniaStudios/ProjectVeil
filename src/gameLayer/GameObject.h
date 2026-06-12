#pragma once
#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <iostream>
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

/** Game Object Data **/
struct GameObject 
{
	GameObject();

	/// Data
	const char* name = "GameObject";
	std::uint64_t id = 0;

	int type = OBJECT_GENERIC;
	/// Debug Display
	bool display3DModel = true;
	bool displayDirection = false;
	bool displayCollider = false;
	
	/// Status
	float lifeSpan = 0;
	float endLife = 0;
	float decayTime = 0;
	void Decay(float time = 0) { decayTime = time; }

	/// Flags
	bool isEnabled = true;
	bool canBeSelected = true;
	bool isAlive = true;
	bool isDestructible = true;
	bool isInteractable = false;

	/// Physics
	RigidBody3D rigidBody3D = {};

	/// Renderer
	Model* model = new Model{};
	Mesh* mesh = new Mesh{};
	Color defaultColor = WHITE;
	Vector4 meshData = Vector4One();
	int meshVariant = MESH_CUBE;
	
	// Developer Menu Texture Test
	int blockID = -1;

	Vector3 getPosition() const { return rigidBody3D.translation; }
	Quaternion getRotation() const { return rigidBody3D.rotation; }
	Vector3 getSize() const { return rigidBody3D.scale; }

	virtual void update(Scene* scene, float deltaTime);
	virtual void render2D();
	virtual void render3D();
	virtual void onEnable();
	virtual void onDisable();
	virtual void onInteract() { std::cout << name << "(base) was Interacted with.\n"; }

	virtual void Destroy();
	virtual void onDestroy(Scene* scene);
	
	virtual void onCollision(const GameObject* collider);

};

/** Interfaces **/

/** Global Check Functions **/
inline ObjectType getType(void* object)
{
	auto check = static_cast<GameObject*>(object);
	return static_cast<ObjectType>(check->type);
}



#endif
