#include "GamePresentation.h"

#include <iostream>

#include <GameObject.h>
#include <LightingSystem.h>
#include <sim/Presentation.h>

namespace {
	class GamePresentation final : public Sim::Presentation
	{
	public:
		void BindVisuals(GameObject& object) override
		{
			// Release whatever this object generated for itself before replacing it..
			object.releaseGeneratedModel();

			// Model: shared AssetManager handle when one is named, else a unit cube
			AssetManager::Asset* modelAsset = object.modelName.empty() ? nullptr : AssetManager::GetAssetPtrByName(object.modelName, AssetManager::ASSET_MODEL);
			if (modelAsset != nullptr && modelAsset->model.meshCount > 0)
			{
				object.model = modelAsset->model;
				object.ownsModel = false;
			}
			else
			{
				if (!object.modelName.empty())
				{
					std::cerr << "Missing model asset '" << object.modelName << "' on " << object.name << " — using cube\n";
				}
				object.model = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
				object.ownsModel = true;
			}

			// Texture: only applied to owned primitives — an asset model's materials
			// are shared by every object using it, and it brings its own .mtl textures
			AssetManager::Asset* textureAsset = object.textureName.empty() ? nullptr : AssetManager::GetAssetPtrByName(object.textureName, AssetManager::ASSET_TEXTURE);
			if (textureAsset != nullptr)
			{
				object.texture = textureAsset->texture;
			}
			else if (!object.textureName.empty())
			{
				std::cerr << "Missing texture asset '" << object.textureName << "' on " << object.name << "\n";
			}

			if (object.ownsModel && object.texture.id != 0)
			{
				object.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = object.texture;
			}

			// Every runtime model binding funnels through here — editor placement,
			// duplication, setModel(), onEnable() and GameMap's Restore* — which makes
			// this the one place that guarantees new geometry is lit.
			Lighting::LightingSystem::getInstance().ApplyToModel(object.model);

			// ...and the one place a mesh collider can be kept honest. Its whole point is
			// to match the art, so it has to be refitted whenever the art changes rather
			// than only when the object is first built. Box and sphere colliders are
			// authored by hand and are deliberately left alone.
			if (object.rigidBody3D.collider.shape == COLLIDER_MESH)
			{
				object.rigidBody3D.collider.FitToModel(object.model);
			}
		}

		void BindTexture(GameObject& object) override
		{
			// Bind without regenerating the model — safe to call every frame
			if (AssetManager::Asset* asset = AssetManager::GetAssetPtrByName(object.textureName, AssetManager::ASSET_TEXTURE))
			{
				object.texture = asset->texture;
				if (object.ownsModel && object.texture.id != 0)
				{
					object.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = object.texture;
				}
			}
		}

		void FreeGeneratedModel(GameObject& object) override
		{
			if (object.model.meshCount > 0) { UnloadModel(object.model); }
		}
	};
}

namespace Client {
	void InstallGamePresentation()
	{
		static GamePresentation instance;
		Sim::InstallPresentation(&instance);
	}
}
