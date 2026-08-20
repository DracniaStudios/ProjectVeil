#include "GameObject.h"

#include <LightingSystem.h>
#include <SceneManager.h>

// A static getBoundingBox(Model, Vector3) used to live here. It had no callers
// — every collision box is built by RigidBody3D::SyncCollisionBox from the
// body's own translation/scale — and its guard was inverted:
// permaAssertComment(mdl.meshCount == 0, "No Meshes In Model") asserts that the
// model has NO meshes, so it would have fired on every valid model and stayed
// quiet on exactly the null-mesh case the next line dereferences. Removed
// rather than corrected: reviving it would reintroduce a second, divergent
// source of truth for how a bounding box is built.

#pragma region GameObject
/** Initialization **/
GameObject::GameObject()
{
	isEnabled = true;
	rigidBody3D = {};

	// Fallback unit cube so the object renders before assets are bound.
	// Scale is applied through model.transform in render3D, so the mesh
	// itself never needs to be regenerated when the object is resized
	if (model.meshCount == 0)
	{
		model = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
		ownsModel = true;
	}

	if (model.materialCount == 0) {
		model.materials[MATERIAL_MAP_DIFFUSE] = LoadMaterialDefault();
	}

	// raylib's DrawMesh() reads material.shader, so lighting has to be written
	// into the material — BeginShaderMode() only affects the rlgl batch and
	// would leave models unlit. loadVisuals() repeats this on every rebind;
	// this call covers an object drawn before its first bind. It is a no-op
	// until LightingSystem::Init() has run.
	LightingSystem::getInstance().ApplyToModel(model);

	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(model.meshes[0]);

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
	// Only primitives this object generated for itself are unloadable. A named
	// asset's model is shared by every object using it and belongs to the
	// AssetManager, so this is a no-op there rather than a free.
	if (!ownsModel) { return; }

	if (model.meshCount > 0) { UnloadModel(model); }
	model = {};
	ownsModel = false;
}

/** Asset Binding **/
void GameObject::loadVisuals()
{
	// Release whatever this object generated for itself before replacing it.
	// Every rebind used to orphan the previous cube's GPU buffers — one leak per
	// projectile fired, per object in a loaded world, and per Entity spawned in
	// the editor. This is safe because ownsModel is only ever true on the sole
	// owner; see the ownership note in GameObject.h for the invariant and the
	// two call sites that maintain it.
	releaseGeneratedModel();

	// Model: shared AssetManager handle when one is named, else a unit cube
	Asset* modelAsset = modelName.empty() ? nullptr : GetAssetPtrByName(modelName, ASSET_MODEL);
	if (modelAsset != nullptr && modelAsset->model.meshCount > 0)
	{
		model = modelAsset->model;
		ownsModel = false;
	}
	else
	{
		if (!modelName.empty())
		{
			std::cerr << "Missing model asset '" << modelName << "' on " << name << " — using cube\n";
		}
		model = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
		ownsModel = true;
	}

	// Texture: only applied to owned primitives — an asset model's materials
	// are shared by every object using it, and it brings its own .mtl textures
	Asset* textureAsset = textureName.empty() ? nullptr : GetAssetPtrByName(textureName, ASSET_TEXTURE);
	if (textureAsset != nullptr)
	{
		texture = textureAsset->texture;
	}
	else if (!textureName.empty())
	{
		std::cerr << "Missing texture asset '" << textureName << "' on " << name << "\n";
	}

	if (ownsModel && texture.id != 0)
	{
		model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
	}

	// Every runtime model binding funnels through here — editor placement,
	// duplication, setModel(), onEnable() and loadCommonFromJson() — which makes
	// this the one place that guarantees new geometry is lit.
	LightingSystem::getInstance().ApplyToModel(model);
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

	// Bind without regenerating the model — safe to call every frame
	if (Asset* asset = GetAssetPtrByName(assetName, ASSET_TEXTURE))
	{
		texture = asset->texture;
		if (ownsModel && texture.id != 0)
		{
			model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
		}
	}
}

/** Lifecycle **/

void GameObject::onEnable()
{
	isEnabled = true;

	// RigidBody3D::Update() latches canCollide false while disabled and never
	// clears it on its own, so re-enabling must restore it explicitly here.
	rigidBody3D.canCollide = true;

	loadVisuals();

	// Set Initial Data (Update() refreshes this from translation/scale)
	rigidBody3D.collisionBox = mesh.vertexCount > 0
		? GetMeshBoundingBox(mesh)
		: GetMeshBoundingBox(model.meshes[0]);

	lifeSpan = 0;
	deathSpan = 1;
}

void GameObject::onDisable()
{
	isEnabled = false;

	// Textures and named models belong to the AssetManager — only unload
	// primitives this object generated for itself
	releaseGeneratedModel();

	if (mesh.vertexCount > 0)
	{
		UnloadMesh(mesh);
		mesh = {};
	}
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
		// Two volumes, because they are genuinely different things once an object
		// is rotated. White is the axis-aligned box the solver actually tests;
		// green is the object's true oriented shape. They coincide exactly while
		// the object is unrotated, and the gap between them at other angles is
		// the slack an AABB solver has to accept — not a bug, but worth seeing.
		DrawBoundingBox(rigidBody3D.collisionBox, WHITE);
		DrawOrientedBoxWires(rigidBody3D.translation, rigidBody3D.scale, rigidBody3D.rotation, GREEN);
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

void GameObject::update(Scene* scene, float deltaTime)
{
	rigidBody3D.isEnabled = isEnabled;
	if (!isEnabled) { return; }

	// Rigidbody Data (Update() refreshes collisionBox from translation/scale)
	{
		rigidBody3D.Update(deltaTime);
	}
	// Sound Data
	{
		if (soundInstance != nullptr && soundInstance->isValid()) {
			FMOD_3D_ATTRIBUTES attributes = get3DAttributes();
			soundInstance->set3DAttributes(&attributes);
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
	// Removal is deferred: Destroy() can be called from inside the update
	// loop that iterates gameMap.gameObjects, and erasing there invalidates
	// the iterators. The scene sweeps pendingDestroy objects after updating.
	isAlive = false;
	pendingDestroy = true;
}

void GameObject::onDestroy(Scene* scene)
{
	// Removal happens in the scene's pendingDestroy sweep (Scene_updateScene);
	// erasing here would invalidate the update loop's iterators
	std::cout << "Destroy Object: " << name << "\n";

	// The destructor is trivial and never frees GPU resources, so a generated
	// primitive's Model/Mesh would otherwise leak the moment this object is
	// erased. onDisable() already does exactly this release, idempotently.
	onDisable();
}

void GameObject::onCollision(const GameObject* collider)
{
	// Collision Data Checks
}

// FMOD requires forward and up to be normalized and perpendicular
FMOD_3D_ATTRIBUTES GameObject::get3DAttributes() const
{
	FMOD_3D_ATTRIBUTES attributes = {};
	attributes.position = Vector3ToFMOD(getPosition());
	attributes.velocity = Vector3ToFMOD(getVelocity());
	attributes.forward = Vector3ToFMOD(Vector3Normalize(rigidBody3D.forward));
	attributes.up = Vector3ToFMOD(Vector3Normalize(rigidBody3D.up));
	return attributes;
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
	j["CanBeSelected"] = canBeSelected;
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
	canBeSelected = j.value("CanBeSelected", true);
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

	// Rebind the runtime handles from the loaded asset names
	loadVisuals();

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
#pragma endregion
