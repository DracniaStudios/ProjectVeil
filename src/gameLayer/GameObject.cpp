#include "GameObject.h"

#include <asserts.h>
#include <gameMap.h>
#include <AssetManager.h>
#include <SceneManager.h>

BoundingBox getBoundingBox(Model mdl, Vector3 pos)
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
	model = new Model();
	mesh = new Mesh();

	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(*mesh);
	
	lifeSpan = 0;
	endLife = 1;

}

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
		SetMaterialTexture(&model->materials[0], MATERIAL_MAP_ALBEDO, getTextureFromID(blockID));
		model->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = getTextureFromID(blockID);
		defaultColor = WHITE;
	}
	
	// Set Initial Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(*mesh);

	lifeSpan = 0;
	endLife = 1;
}

void GameObject::onDisable()
{
	isEnabled = false;
	UnloadMesh(*mesh);
	UnloadModel(*model);
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
		DrawModel(*model, rigidBody3D.translation, 1.0f, defaultColor);
		DrawModelWires(*model, rigidBody3D.translation, 1.0f, BLACK);
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

	// Update Data
	rigidBody3D.collisionBox = GetMeshBoundingBox(*mesh);
	rigidBody3D.Update(deltaTime);

	lifeSpan += deltaTime;

	if (isAlive) { endLife += deltaTime; }

	if (!isAlive) {
		if (lifeSpan > endLife) {
			Destroy();
		}
	}
}

float setLife(float life, int time) { return static_cast<float>(life + time); }

void GameObject::Destroy()
{
	isAlive = false;
	onDestroy(SceneManager::getInstance().currentScene);
}

// Destroy Object After Delay (in milliseconds)
void GameObject::onDestroy(Scene* scene)
{
	scene->gameMap.removeObject(this);
	std::cout << "Destroy Object: " << name << "\n";
}

void GameObject::onCollision(const GameObject* collider)
{
	// Collision Data Checks
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
	defaultColor = Color{
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		255
	};
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
	// Load Common Object Data
	j["RigidBody3D"] = rigidBody3D.formatToJson();
	j["Life"] = lifeSpan;
	j["ObjectType"] = getType();
}

bool GameObject::loadCommonFromJson(Json& j)
{

	if (j.contains("RigidBody3D"))
	{
		auto json = j["RigidBody3D"];

		if (json.is_object())
		{
			if (!rigidBody3D.loadFromJson(j))
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

	if (j["Life"].is_number())
	{
		lifeSpan = j[lifeSpan];
	}
	return true;
}


#pragma endregion
