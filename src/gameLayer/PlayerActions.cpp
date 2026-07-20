#include <Player.h>

#include <SceneManager.h>

// Projectile Variant
void Player::Fire()
{
	//
}

void Player::FireLaser()
{
	auto camera = SceneManager::getInstance().currentScene->camera;
	// Create Ray from Camera
	Ray cameraRay = { camera->position, camera->forward };

	// Check Ray Collision with Game Objects

	auto scene = SceneManager::getInstance().currentScene;

	// gameMap.gameObjects stores plain GameObjects (anything saved there was
	// sliced), so casting them to Entity* to deal damage wrote past the end
	// of the object — only push them around
	for (auto& obj : scene->gameMap.gameObjects)
	{
		if (GetRayCollisionBox(cameraRay, obj.rigidBody3D.collisionBox).hit)
		{
			obj.onCollision(this);
			obj.rigidBody3D.AddForce(camera->forward, 0.1f);
		}
	}

	// Real entities live in scene->entities and can take damage
	for (auto& [id, entity] : scene->entities)
	{
		if (GetRayCollisionBox(cameraRay, entity->rigidBody3D.collisionBox).hit)
		{
			entity->applyHealthValue(baseDamage * 0.1f, true);
			entity->rigidBody3D.AddForce(camera->forward, 0.1f);
		}
	}
}

void Player::Interact()
{
	auto scene = SceneManager::getInstance().currentScene;
	if (scene->is2DActive) { return; }

	// Check buff for increased range
	if (auto* rangeBuff = getBuff(BUFF_RANGE);
		rangeBuff && rangeBuff->remaining_time() > 0) {
		interactRange = 10;
	}
	else {
		interactRange = 5;
	}

	const auto cameraRay = Ray(scene->camera->position, scene->camera->forward);
	for (auto& interactable : scene->interactables)
	{
		const auto obj = interactable.second.get();
		const GameObject* decoy = nullptr;
		for (auto& sceneObject : scene->gameMap.gameObjects) { if (sceneObject.id == obj->id) { decoy = &sceneObject; } }
		if (decoy == nullptr) { continue; }

		// interactRange
		auto distance = Vector3Distance(cameraRay.position, decoy->getPosition());

		//Check interaction
		if (GetRayCollisionBox(cameraRay, decoy->rigidBody3D.collisionBox).hit && distance <= interactRange)
		{
			if (obj->isInteractable)
			{
				obj->onInteract();
				return;
			}
		}
	}
	
	if (artifact->isEnabled) { scene->SetMiniGame(artifactMode); return; }
}

static GameObject* selectObject(const Camera& cam)
{
	auto scene = SceneManager::getInstance().currentScene;
	auto player = scene->player;

	Ray selectRay = {
		cam.position,
		scene->camera->forward
	};

	if (!scene->gameMap.gameObjects.empty())
	{
		for (auto& object : scene->gameMap.gameObjects)
		{
			if (object.isEnabled)
			{
				BoundingBox objectBox = {
					object.getPosition() - object.getSize() / 2,
					object.getPosition() + object.getSize() / 2
				};
				if (GetRayCollisionBox(selectRay, objectBox).hit)
				{
					if (!object.canBeSelected) { continue; }

					return &object;
				}
			}
		}
	}

	return nullptr;

}