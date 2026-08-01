#include <Player.h>

#include <SceneManager.h>

void Player::Interact()
{
	const auto scene = SceneManager::getInstance().currentScene;
	const auto camera = &SceneManager::getInstance().camera3D;
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

	const auto cameraRay = Ray(camera->position, this->camera.forward);
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