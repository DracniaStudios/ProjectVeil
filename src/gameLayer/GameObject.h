#pragma once
#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <iostream>
#include <Physics.h>

struct Scene;

enum ObjectType : uint8_t
{
	OBJECT_GENERIC,
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
	float endLife = 1;
	float decayTime = 1;
	void Decay(float time = 1) { decayTime = time; }

	/// Flags
	bool isEnabled = true;
	bool canBeSelected = true;
	bool isAlive = true;
	bool isDestructible = true;

	/// Physics
	RigidBody3D rigidBody3D = {};

	/// Renderer
	Model model = {};
	Mesh mesh = {};
	Texture2D texture = {};
	Color defaultColor = WHITE;

	Vector3 getPosition() const { return rigidBody3D.translation; }
	Quaternion getRotation() const { return rigidBody3D.rotation; }
	Vector3 getSize() const { return rigidBody3D.scale; }
	ObjectType getType() const { return static_cast<ObjectType>(type); }

	virtual void update(Scene* scene, float deltaTime);
	virtual void render2D();
	virtual void render3D();
	virtual void onEnable();
	virtual void onDisable();

	// Update State
	virtual void Destroy();
	virtual void onDestroy(Scene* scene);
	virtual void onCollision(const GameObject* collider);
	

	// Save Data
	virtual Json formatToJson() { return {}; }
	virtual bool loadFromJson(Json& j) { return true; }
	void addCommonToJson(Json& j);
	bool loadCommonFromJson(Json& j);


};

/** Interfaces **/

enum InteractionType
{
	INTERACT_MINIGAME,
	INTERACT_OBJECT,
	INTERACT_ITEM,
};

struct InteractableObject : GameObject
{
private:
	int interactType = 0;
	int interactValue = 0;
public:

	InteractableObject(InteractionType type, int value = 0);
	bool isInteractable = true;
	virtual void onInteract();
};


/** Global Check Functions **/
inline ObjectType getType(void* object)
{
	auto check = static_cast<GameObject*>(object);
	return static_cast<ObjectType>(check->type);
}



#endif
