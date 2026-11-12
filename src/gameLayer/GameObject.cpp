#include "GameObject.h"

#include <Client/DebugDraw.h>
#include <sim/Presentation.h>

// Mesh geometry reaches collision through the collider, never straight from the
// model. 

#pragma region GameObject
/** Initialization **/
GameObject::GameObject()
{
	isEnabled = true;
	rigidBody3D = {};
	lifeSpan = 0;
	deathSpan = 1;

}

/** Model Ownership **/
void GameObject::disownModel()
{
	// Deliberately does not unload: the object this handle was copied from is
	// still using it. Clearing `model` as well means the next loadVisuals()
	// cannot mistake an inherited handle for one of its own.
	model = {};
	ownsModel = false;
}

void GameObject::releaseGeneratedModel()
{
	// Only primitives this object generated for itself are unloadable.
	if (!ownsModel) { return; }

	Sim::CurrentPresentation().FreeGeneratedModel(*this);
	model = {};
	ownsModel = false;
}

/** Asset Binding **/
void GameObject::loadVisuals()
{
	Sim::CurrentPresentation().BindVisuals(*this);
}

void GameObject::setModel(const std::string& assetName)
{
	if (modelName == assetName && model.meshCount > 0) { return; }
	modelName = assetName;
	loadVisuals();
}

void GameObject::setTexture(const std::string& assetName)
{
	textureName = assetName;
	Sim::CurrentPresentation().BindTexture(*this);
}

/** Lifecycle **/

void GameObject::onEnable()
{
	isEnabled = true;

	// RigidBody3D::Update() latches canCollide false while disabled and never
	// clears it on its own, so re-enabling must restore it explicitly here.
	rigidBody3D.collider.canCollide = true;
	rigidBody3D.collider.SetObjectID(id);

	// With a presentation installed, fits a mesh collider to whatever model this
	// binds; then refreshes the box
	loadVisuals();
	rigidBody3D.SyncBroadPhaseBox();

	lifeSpan = 0;
	deathSpan = 1;
}

void GameObject::onDisable()
{
	isEnabled = false;
	rigidBody3D.collider.canCollide = false;
	// Textures and named models belong to the AssetManager — only unload
	// primitives this object generated for itself
	releaseGeneratedModel();
}


void GameObject::render2D()
{
	if (!isEnabled) { return; }
}
void GameObject::render3D()
{
	if (!isEnabled) { return; }

	if (displayCollider)
	{
		// Two volumes, because they are genuinely different things. White is the
		// axis-aligned box the BROAD phase tests; the coloured one is the collider
		// the narrow phase actually tests. They coincide exactly while the object is
		// unrotated and its collider is the default, and every gap between them —
		// from rotation, from a resized collider, from an offset one — is real slack
		// worth being able to see.
		DrawBoundingBox(rigidBody3D.broadPhaseBox, WHITE);

		// Colour carries the mode. A trigger and a solid are indistinguishable on
		// screen otherwise and behave completely differently, which is a miserable
		// thing to debug by moving objects around until something stops falling.
		const Collider3D& collider = rigidBody3D.collider;
		const Color colliderColor = collider.isTrigger() ? SKYBLUE : GREEN;
		const Vector3 colliderCenter = rigidBody3D.GetColliderCenter();

		if (collider.shape == COLLIDER_SPHERE)
		{
			DrawSphereWires(colliderCenter, collider.GetWorldRadius(rigidBody3D.scale), 8, 8, colliderColor);
		}
		else
		{
			DrawOrientedBoxWires(colliderCenter,
				Vector3Scale(rigidBody3D.GetLocalHalfExtents(), 2.0f),
				rigidBody3D.rotation, colliderColor);
		}
	}

	if (display3DModel) {
		// Bake scale + orientation into the transform — meshes stay unit-sized,
		// so both generated cubes and .obj models follow rigidBody3D.scale
		Vector3 scale = rigidBody3D.scale;
		model.transform = MatrixMultiply(
			MatrixScale(scale.x, scale.y, scale.z),
			QuaternionToMatrix(rigidBody3D.rotation));
		DrawModel(model, rigidBody3D.translation, 1.0f, defaultColor);
		DrawModelWires(model, rigidBody3D.translation, 1.0f, BLACK);
	}

	if (displayDirection) {
		/// Show Directions
		DrawSphere(rigidBody3D.forward + rigidBody3D.translation, 0.1f, RED);
		DrawSphere(rigidBody3D.back + rigidBody3D.translation, 0.1f, ORANGE);
		DrawSphere(rigidBody3D.left + rigidBody3D.translation, 0.1f, YELLOW);
		DrawSphere(rigidBody3D.right + rigidBody3D.translation, 0.1f, GREEN);
		DrawSphere(rigidBody3D.up + rigidBody3D.translation, 0.1f, BLUE);
		DrawSphere(rigidBody3D.down + rigidBody3D.translation, 0.1f, PURPLE);
	}
}

void GameObject::update(SceneManagement::Scene* scene, float deltaTime)
{
	// A body that has just been disabled stops taking part in the physics world,
	// which then reports the contacts it had as ended
	rigidBody3D.isEnabled = isEnabled;
	if (!isEnabled)
	{
		// RigidBody3D::Update() is what clears canCollide for a disabled body,
		// but returning here means it never runs. PhysicsWorld::Solve skips a
		// disabled body either way; this keeps anything else that asks the
		// collider from treating it as solid. onEnable() restores the flag.
		rigidBody3D.collider.canCollide = false;
		return;
	}

	// Rigidbody Data (Update() refreshes broadPhaseBox from the collider)
	{
		rigidBody3D.Update(deltaTime);
	}
	// Sound Data
	{
		if (soundInstance != nullptr) {
			Sim::CurrentPresentation().FollowObjectSound(*this);
		}
	}


	// Destroy Object
	{
		lifeSpan += deltaTime;
		if (isAlive) { deathSpan += deltaTime; }

		if (!isAlive) {
			if (lifeSpan > deathSpan) {
				Destroy();
			}
		}
	}
}

void GameObject::Destroy()
{
	// Removal is deferred, for every kind of object: Destroy() can be called from
	// inside the update loop that iterates the GameMap, and erasing there
	// invalidates the iterators. GameMap::SweepDestroyed removes pendingDestroy
	// objects after the updates.
	isAlive = false;
	pendingDestroy = true;
}

void GameObject::onDestroy(SceneManagement::Scene* scene)
{
	// Called by GameMap::SweepDestroyed just before it erases this object
	std::cout << "Destroy Object: " << name << "\n";

	// The destructor is trivial and never frees GPU resources, so a generated
	// primitive's Model/Mesh would otherwise leak the moment this object is
	// erased. onDisable() already does exactly this release, idempotently.
	onDisable();
}

#pragma endregion

/** GameObject Save Data **/
#pragma region Save GameObject

void GameObject::addCommonToJson(Json& j)
{
	// Save Common Object Data
	j["RigidBody3D"] = rigidBody3D.formatToJson();
	j["ObjectType"] = getType();
	j["Name"] = name;

	// Flags
	j["IsEnabled"] = isEnabled;
	j["IsSelectable"] = isSelectable;
	j["IsDestructible"] = isDestructible;

	// Renderer
	j["Color"] = { defaultColor.r, defaultColor.g, defaultColor.b, defaultColor.a };
	j["CastsShadow"] = castsShadow;

	if (!textureName.empty()) { j["Texture"] = textureName; }
	if (!modelName.empty()) { j["Model"] = modelName; }
}

bool GameObject::loadCommonFromJson(Json& j)
{

	if (j.contains("RigidBody3D"))
	{
		auto json = &j["RigidBody3D"];

		if (json->is_object())
		{
			if (!rigidBody3D.loadFromJson(*json))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	type = static_cast<ObjectType>(j.value("ObjectType", static_cast<int>(OBJECT_GENERIC)));
	name = j.value("Name", name);

	// Flags
	isEnabled = j.value("IsEnabled", true);
	isSelectable = j.value("IsSelectable", j.value("CanBeSelected", true));
	isDestructible = j.value("IsDestructible", true);

	// Renderer
	if (j.contains("Color") && j["Color"].is_array() && j["Color"].size() == 4)
	{
		defaultColor.r = j["Color"][0];
		defaultColor.g = j["Color"][1];
		defaultColor.b = j["Color"][2];
		defaultColor.a = j["Color"][3];
	}

	// Defaults to true so worlds saved before the lighting system still cast
	// shadows instead of silently going flat
	castsShadow = j.value("CastsShadow", true);

	if (j.contains("Texture") && j["Texture"].is_string())
	{
		textureName = j["Texture"].get<std::string>();
	}

	if (j.contains("Model") && j["Model"].is_string())
	{
		modelName = j["Model"].get<std::string>();
	}

	// Visuals are bound by GameMap's Restore*, once, not here: loading makes no GPU call

	return true;
}

Json GameObject::formatToJson()
{
	Json j;
	addCommonToJson(j);
	return j;
}

bool GameObject::loadFromJson(Json& j)
{
	return loadCommonFromJson(j);
}

void GameObject::DrawGizmos() {

}

#pragma endregion
