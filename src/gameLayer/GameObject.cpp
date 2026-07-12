#include "GameObject.h"

#include <asserts.h>
#include <gameMap.h>
#include <AssetManager.h>
#include <SceneManager.h>
#include <AudioManager.h>

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

	// Load Model else Cube. rigidBody3D.scale is still zero-initialized here
	// (it defaults to One later), so building the cube from it produced a
	// degenerate, invisible mesh — use a unit cube to match the default scale
	if (model.meshCount == 0)
	{
		model = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
	}

	if (texture == nullptr) {
		texture = &AssetManager::getInstance().frame;
	}


	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(mesh);
	
	lifeTime = 0;
	deathTime = 1;
	
}

/** Lifecycle **/

void GameObject::onEnable()
{
	isEnabled = true;

	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(mesh);

	if (texture == nullptr) {
		texture = &AssetManager::getInstance().frame;
	}
	model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture->texture;

	lifeTime = 0;
	deathTime = 1;
}

void GameObject::onDisable()
{
	isEnabled = false;
	UnloadTexture(model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture);
	UnloadMesh(mesh);
	UnloadModel(model);
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
		// Apply the body's orientation so spinning/tumbling objects render rotated
		model.transform = QuaternionToMatrix(rigidBody3D.rotation);
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
		lifeTime += deltaTime;
		if (isAlive) { deathTime += deltaTime; }

		if (!isAlive) {
			if (lifeTime > deathTime) {
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
#pragma region Interactable Object
InteractableObject::InteractableObject(const InteractionType interact, int value)
{
	interactType = interact;
	interactValue = value;
}

void InteractableObject::onInteract()
{
	auto rng = std::ranlux24_base(std::random_device{}());
	defaultColor = getRandomColor(rng);

	//AudioManager::getInstance().Play("anime_wow");
	AudioManager::getInstance().PlayEvent3D(interactSound, *this);
	AudioManager::getInstance().Play("anime_wow");

	if (interactType == INTERACT_MINIGAME)
	{
		SceneManager::getInstance().currentScene->SetMiniGame(interactValue);
		return;
	}
	
	if (interactType == INTERACT_OBJECT)
	{
		// Activate Specific Object ID
		return;
	}
	
	if (interactType == INTERACT_ITEM)
	{
		// Give Player Specific Item
		return;
	}
}
#pragma endregion



/** GameObject Save Data **/
#pragma region Save GameObject

void GameObject::addCommonToJson(Json& j)
{
	// Save Common Object Data
	j["RigidBody3D"] = rigidBody3D.formatToJson();
	j["Life"] = lifeTime;
	j["ObjectType"] = getType();
	j["Name"] = name;

	// Flags
	j["IsEnabled"] = isEnabled;
	j["CanBeSelected"] = canBeSelected;
	j["IsDestructible"] = isDestructible;

	// Renderer
	j["Color"] = { defaultColor.r, defaultColor.g, defaultColor.b, defaultColor.a };
	if (texture != nullptr && !texture->name.empty())
	{
		j["Texture"] = texture->name;
	}
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
		lifeTime = j["Life"];
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
		if (auto asset = GetAssetPtrByName(j["Texture"].get<std::string>()))
		{
			texture = asset;
		}
	}

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
