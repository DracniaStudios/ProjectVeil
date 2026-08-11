#pragma once
#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <iostream>
#include <string>
#include <helpers.h>
#include <Physics.h>
#include <AssetManager.h>
#include <AudioManager.h>

struct Scene;

enum ObjectType
{
	OBJECT_GENERIC,
	OBJECT_PLAYER,
	OBJECT_ENTITY,
	OBJECT_INTERACTABLE,
	OBJECT_ITEM,
	OBJECT_PROJECTILE,
	OBJECT_ENVIRONMENT,
	OBJECT_COUNT
};

/** Game Object Data **/
struct GameObject 
{
	GameObject();
	~GameObject() = default;

	/// Data
	std::string name = "GameObject";
	std::uint64_t id = 0;

	ObjectType type = OBJECT_GENERIC;
	/// Debug Display
	bool display3DModel = true;
	bool displayDirection = false;
	bool displayCollider = false;
	bool isSelectable = true;// WorldEditor
	
	/// Flags
	bool isEnabled = true;
	bool canBeSelected = true;
	bool isAlive = true;
	bool isDestructible = true;
	bool pendingDestroy = false; // removal is deferred to the end of the frame
	bool ownsModel = false; // generated primitive (unloadable) vs shared AssetManager model
	bool castsShadow = true; // Shadow Casting is based on each object and rendered in the shadow pass.
	
	/// Status
	float lifeSpan = 0;
	float deathSpan = 1;
	float decayTime = 1;
	void Decay(float time = 1) { decayTime = time; }

	// Lives on GameObject (not Entity) so projectiles stored by value in
	// GameMap::gameObjects keep their damage after slicing to GameObject
	float baseDamage = 1.0f;

	/// Physics
	RigidBody3D rigidBody3D = {};

	/// Renderer
	// Assets are referenced by AssetManager name so they can be saved and
	// reloaded; the raylib handles below are rebound from them at runtime
	std::string modelName = "";   // empty = generated unit cube
	std::string textureName = ""; // empty = material default
	Model model = {};
	Mesh mesh = {};
	Texture texture = {};
	Color defaultColor = WHITE;


	// Record the asset by name and rebind the runtime handle
	void setModel(const std::string& assetName);
	void setTexture(const std::string& assetName);
	// Rebind model + texture handles from modelName/textureName
	void loadVisuals();

	/**
	 * Model ownership.
	 *
	 * `ownsModel` means "this object generated this model and is its sole owner".
	 * The implicit copy constructor copies both `model` and `ownsModel`, so a
	 * copy would otherwise claim a handle its source is still using. The
	 * invariant that keeps releasing safe is therefore:
	 *
	 *   no two *live* objects may hold the same handle with ownsModel == true
	 *
	 * Copies are the only way to break it, so any code that copies a GameObject
	 * and then binds the copy into the scene must call disownModel() first.
	 * There are exactly two such places, both in the World Editor:
	 * WorldEditor::SpawnStagedObject and WorldEditor::DuplicateSelection.
	 */

	// Drops an inherited handle without freeing it — the original still owns it
	void disownModel();

	// Frees a model this object generated for itself. A no-op on a shared
	// AssetManager model, which this object does not own.
	void releaseGeneratedModel();

	// Audio
	FMOD::Studio::EventInstance* soundInstance = nullptr;
	std::string defaultSound = "test_beep";
	std::string soundParameterName = "Parameter";
	int soundParameterValue = 0;
	bool isLooping = false; // Add File Save
	FMOD_3D_ATTRIBUTES get3DAttributes() const;

	Vector3 getPosition() const { return rigidBody3D.translation; }
	Quaternion getRotation() const { return rigidBody3D.rotation; }
	Vector3 getVelocity() const { return rigidBody3D.GetVelocity(); }
	Vector3 getSize() const { return rigidBody3D.scale; }
	ObjectType getType() const { return type; }

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
	virtual Json formatToJson();
	virtual bool loadFromJson(Json& j);
	void addCommonToJson(Json& j);
	bool loadCommonFromJson(Json& j);
};

/** Interfaces **/
enum InteractionType
{
	INTERACT_NONE = 0,
	INTERACT_MINIGAME,
	INTERACT_UNLOCK,
	INTERACT_ITEM
};

struct InteractableObject : public GameObject
{
public:
	InteractableObject() = default; // Default constructor
	InteractableObject(InteractionType type, int value);
	InteractableObject(InteractionType type, int value, int activator);

	InteractionType interactType = INTERACT_NONE;
	int interactValue = 0;
	int activatorValue = 0;
	bool isInteractable = true;
	bool isRunningMiniGame = false;

	virtual void render3D() override {
		GameObject::render3D();
	};
	virtual void update(Scene* scene, float deltaTime) override
	{
		GameObject::update(scene, deltaTime);
	};
	virtual void onEnable() override {
		GameObject::onEnable();
	};
	virtual void onDisable() override {
		GameObject::onDisable();
	};
	virtual void onInteract();

	// Save Data
	Json formatToJson() override;
	bool loadFromJson(Json& j) override;
};


/** Global Check Functions **/
inline ObjectType getType(void* object)
{
	auto check = static_cast<GameObject*>(object);
	return static_cast<ObjectType>(check->type);
}

inline const char* interactTypeToString(InteractionType type) {
	switch (type) {
		case INTERACT_MINIGAME: return "Mini Game";
		case INTERACT_UNLOCK: return "Unlock";
		case INTERACT_ITEM: return "Item";
		default: return "None";
	}
}

inline const char* objectTypeToString(int type)
{
	switch (type)
	{
	case OBJECT_PLAYER:      return "Player";
	case OBJECT_ENTITY:      return "Entity";
	case OBJECT_ITEM:        return "Item";
	case OBJECT_PROJECTILE:  return "Projectile";
	case OBJECT_ENVIRONMENT: return "Environment";
	default:                 return "Generic";
	}
}


#endif
