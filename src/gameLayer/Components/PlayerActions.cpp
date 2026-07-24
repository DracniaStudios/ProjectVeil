#include <Player.h>

#include <SceneManager.h>

// Projectile Variant
void Player::Fire()
{
	std::cout << "Player Fire Projectile \n";
}

void Player::FireLaser()
{
	const auto camera = &SceneManager::getInstance().currentScene->player->camera;
	// Create Ray from Camera
	Ray cameraRay = { camera->position, camera->forward };

	// Check Ray Collision with Game Objects

	const auto scene = SceneManager::getInstance().currentScene;

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
	const auto scene = SceneManager::getInstance().currentScene;
	auto interactRange = 5;

	if (scene->is2DActive) { return; }

	// Check buff for increased range
	if (auto* rangeBuff = getBuff(BUFF_RANGE);
		rangeBuff && rangeBuff->remaining_time() > 0) {
		interactRange = extendedInteractRange;
	}
	else {
		interactRange = defaultInteractRange;
	}

	const auto cameraRay = Ray(camera.position, camera.forward);
	for (auto& interactable : scene->interactables)
	{
		const auto obj = interactable.second.get();
		auto distance = Vector3Distance(cameraRay.position, obj->getPosition());

		//Check interaction
		if (GetRayCollisionBox(cameraRay, obj->rigidBody3D.collisionBox).hit && distance <= interactRange)
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