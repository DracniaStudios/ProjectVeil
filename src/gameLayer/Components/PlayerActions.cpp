#include <Player.h>

#include <SceneManager.h>

void Player::Interact()
{
	const auto scene = SceneManager::getInstance().currentScene;
	const auto camera = &SceneManager::getInstance().camera3D;

	if (scene->is2DActive) { return; }

	// Check buff for increased range
	int interactRange = defaultInteractRange;
	if (auto* rangeBuff = getBuff(BUFF_RANGE);
		rangeBuff && rangeBuff->remaining_time() > 0) {
		interactRange = extendedInteractRange;
	}

	const auto cameraRay = Ray(camera->position, this->camera.forward);

	// Pick the CLOSEST candidate along the ray rather than the first one found.
	// scene->interactables is an unordered_map, so "first" was whatever the hash
	// order happened to be: standing in front of two boxes activated an
	// arbitrary one of them, and the choice could change between runs. Ranking
	// by the ray's own hit distance also stops the player reaching through a
	// near object to trigger one behind it.
	InteractableObject* closest = nullptr;
	float closestDistance = 0.0f;

	for (auto& [id, owned] : scene->gameMap.interactables)
	{
		const auto obj = owned.get();

		// A disabled interactable is one that has been consumed — picked up into
		// the inventory, or switched off by an activator. It draws nothing
		// (render3D returns early on !isEnabled) but keeps its collision box, so
		// without this the player could still interact with thin air.
		if (!obj->isInteractable || !obj->isEnabled) { continue; }

		const RayCollision hit = GetRayCollisionBox(cameraRay, obj->rigidBody3D.collisionBox);
		if (!hit.hit || hit.distance > static_cast<float>(interactRange)) { continue; }

		if (closest == nullptr || hit.distance < closestDistance)
		{
			closest = obj;
			closestDistance = hit.distance;
		}
	}

	if (closest != nullptr)
	{
		closest->onInteract();
		return;
	}

	if (artifact && artifact->isEnabled) { scene->SetMiniGame(artifactMode); return; }
}