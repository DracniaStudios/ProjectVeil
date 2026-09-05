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
	InteractableObject* closest = nullptr;
	float closestDistance = 0.0f;

	for (auto& [id, owned] : scene->gameMap.interactables)
	{
		const auto obj = owned.get();

		if (!obj->isInteractable || !obj->isEnabled) { continue; }

		// The collider, not the box around it, so the crosshair has to actually be
		// on the object rather than merely within its bounds.
		float distance = 0.0f;
		Vector3 normal = {};
		if (!obj->rigidBody3D.Raycast(cameraRay, distance, normal)) { continue; }
		if (distance > static_cast<float>(interactRange)) { continue; }

		if (closest == nullptr || distance < closestDistance)
		{
			closest = obj;
			closestDistance = distance;
		}
	}

	if (closest != nullptr)
	{
		closest->onInteract();
		return;
	}

	if (artifact && artifact->isEnabled) { scene->SetMiniGame(artifactMode); return; }
}