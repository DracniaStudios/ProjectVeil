#include "GamePresentation.h"

#include <iostream>

#include <AssetManager.h>
#include <AudioManager.h>
#include <GameObject.h>
#include <Render/Render.h>
#include <Scene.h>
#include <sim/Presentation.h>

namespace Client {
	std::optional<BoundingBox> LocalBounds(const GameObject& object)
	{
		// BindVisuals gives a model asset to an object only when it loaded, and the
		// shared unit cube otherwise; usesUnitCube records which of the two it did
		if (object.usesUnitCube) { return Render::kUnitCubeBounds; }

		const AssetManager::Asset* asset = object.modelName.empty() ? nullptr : AssetManager::GetAssetPtrByName(object.modelName, AssetManager::ASSET_MODEL);
		if (asset != nullptr) { return asset->localBounds; }
		return std::nullopt;
	}
}

namespace {
	class GamePresentation final : public Sim::Presentation
	{
	public:
		void BindVisuals(GameObject& object) override
		{
			// Model: shared AssetManager handle when one is named, else the renderer's unit cube
			AssetManager::Asset* modelAsset = object.modelName.empty() ? nullptr : AssetManager::GetAssetPtrByName(object.modelName, AssetManager::ASSET_MODEL);
			if (modelAsset != nullptr && modelAsset->model)
			{
				object.model = modelAsset->model;
				object.usesUnitCube = false;
			}
			else
			{
				if (!object.modelName.empty())
				{
					std::cerr << "Missing model asset '" << object.modelName << "' on " << object.name << " — using cube\n";
				}
				object.model = Render::UnitCube();
				object.usesUnitCube = true;
			}

			// Texture: MakeModelDraw shows it on the unit cube only — an asset model's
			// materials are shared by every object using it, and it brings its own .mtl textures
			AssetManager::Asset* textureAsset = object.textureName.empty() ? nullptr : AssetManager::GetAssetPtrByName(object.textureName, AssetManager::ASSET_TEXTURE);
			if (textureAsset != nullptr)
			{
				object.texture = textureAsset->texture;
			}
			else if (!object.textureName.empty())
			{
				std::cerr << "Missing texture asset '" << object.textureName << "' on " << object.name << "\n";
			}

			// ...and the one place a mesh collider can be kept honest. Its whole point is
			// to match the art, so it has to be refitted whenever the art changes rather
			// than only when the object is first built. Box and sphere colliders are
			// authored by hand and are deliberately left alone.
			if (object.rigidBody3D.collider.shape == COLLIDER_MESH)
			{
				if (auto bounds = Client::LocalBounds(object)) { object.rigidBody3D.collider.FitToBounds(*bounds); }
			}
		}

		void BindTexture(GameObject& object) override
		{
			// Bind without regenerating the model — safe to call every frame
			if (AssetManager::Asset* asset = AssetManager::GetAssetPtrByName(object.textureName, AssetManager::ASSET_TEXTURE))
			{
				object.texture = asset->texture;
			}
		}

		void PlayGameplaySoundAt(std::string_view name, Vector3 position, float volume) override
		{
			AudioManager::AudioManager::getInstance().Play3D(std::string(name), position, AudioManager::AUDIO_GAMEPLAY_SFX, volume);
		}

		void PlayObjectSound(GameObject& object) override
		{
			AudioManager::AudioManager::getInstance().Play3D(object.defaultSound, object);

			// Play3D only populates soundInstance when it falls back to the FMOD Studio event path;
			// a plain loaded sound (the common case) leaves it null.
			if (object.soundInstance != nullptr) {
				object.soundInstance->setParameterByName(object.soundParameterName.c_str(), object.soundParameterValue);
			}
		}

		void FollowObjectSound(GameObject& object) override
		{
			if (object.soundInstance != nullptr && object.soundInstance->isValid()) {
				FMOD_3D_ATTRIBUTES attributes = AudioManager::AttributesOf(object);
				object.soundInstance->set3DAttributes(&attributes);
			}
		}

		void OnSceneDeleting(SceneManagement::Scene& scene) override
		{
			scene.QuitMiniGame();
			AudioManager::AudioManager::getInstance().StopAudio();
		}

		void OnFullWipe(SceneManagement::Scene& scene) override
		{
			// Any MiniGame in progress belongs to the round that just ended
			scene.miniGameSession.Quit();
			scene.overlays.Close(Overlay::MiniGame);
			scene.stationRequests.clear();
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
