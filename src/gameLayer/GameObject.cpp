#include "GameObject.h"

#include <SceneManager.h>

static Color colliderColor;

static BoundingBox getBoundingBox(Model mdl, Vector3 pos)
{
	permaAssertComment(mdl.meshes == nullptr, "No Meshes In Model");
	
	BoundingBox box = GetMeshBoundingBox(mdl.meshes[0]);
	box.min = Vector3Add(pos, box.min);
	box.max = Vector3Add(pos, box.max);
	return box;
}

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

	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(model.meshes[0]);

	lifeSpan = 0;
	deathSpan = 1;

}

/** Asset Binding **/

void GameObject::loadVisuals()
{
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
		// Fresh cube per rebind: object copies share the previous handle
		// (and its material), so it can be neither reused nor unloaded here
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
	if (ownsModel && model.meshCount > 0)
	{
		UnloadModel(model);
		model = {};
		ownsModel = false;
	}
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

	if (displayCollider) { DrawBoundingBox(rigidBody3D.collisionBox, colliderColor); }

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

	auto displayRay = [&](Ray ray, Color color)
	{
		DrawLine3D(ray.position, Vector3Add(ray.position, Vector3Scale(ray.direction, 0.5f)), color);
	};
}

void GameObject::update(Scene* scene, float deltaTime)
{
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

static float setLife(float life, int time) { return static_cast<float>(life + time); }

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



//****** Interactable Objects *************//

/** GameObject Save Data **/
#pragma region Save GameObject

void GameObject::addCommonToJson(Json& j)
{
	// Save Common Object Data
	j["RigidBody3D"] = rigidBody3D.formatToJson();
	j["Life"] = lifeSpan;
	j["ObjectType"] = getType();
	j["Name"] = name;

	// Flags
	j["IsEnabled"] = isEnabled;
	j["CanBeSelected"] = canBeSelected;
	j["IsDestructible"] = isDestructible;

	// Renderer
	j["Color"] = { defaultColor.r, defaultColor.g, defaultColor.b, defaultColor.a };

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

	if (j.contains("Life") && j["Life"].is_number())
	{
		lifeSpan = j["Life"];
	}

	type = j.value("ObjectType", static_cast<int>(OBJECT_GENERIC));
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

Json InteractableObject::formatToJson()
{
	Json j;
	addCommonToJson(j);
	j["InteractType"] = interactType;
	j["InteractValue"] = interactValue;
	j["IsInteractable"] = isInteractable;
	return j;
}

bool InteractableObject::loadFromJson(Json& j)
{
	if (!loadCommonFromJson(j)) { return false; }
	interactType = j.value("InteractType", 0);
	interactValue = j.value("InteractValue", 0);
	isInteractable = j.value("IsInteractable", true);
	return true;
}

#pragma endregion
