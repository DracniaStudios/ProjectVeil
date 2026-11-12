#include <Player.h>

#include <SceneManager.h>

void Player::Interact(SceneManagement::Scene& scene)
{
	std::cout << "\n[PLayer.cpp] Interacted with Object\n";
	// Check buff for increased range
	int interactRange = defaultInteractRange;
	if (auto* rangeBuff = getBuff(BUFF_RANGE);
		rangeBuff && rangeBuff->remaining_time() > 0) {
		interactRange = extendedInteractRange;
	}

	// From the player's own eye along their own look, never the client's camera:
	// the server has no camera, and a joined player's camera is someone else's
	const auto cameraRay = Ray(EyePosition(), this->camera.forward);

	// Pick the CLOSEST candidate along the ray rather than the first one found.
	InteractableObject* closest = nullptr;
	float closestDistance = 0.0f;

	scene.gameMap.ForEachInteractable([&](InteractableObject& obj)
	{
		if (!obj.isInteractable || !obj.isEnabled) { return; }

		// The collider, not the box around it, so the crosshair has to actually be
		// on the object rather than merely within its bounds.
		float distance = 0.0f;
		Vector3 normal = {};

		if (!obj.rigidBody3D.Raycast(cameraRay, distance, normal)) { return; }
		if (distance > static_cast<float>(interactRange)) { return; }

		if (closest == nullptr || distance < closestDistance)
		{
			std::cout << "\n[PlayerActions.cpp] Interaction Applied to " << obj.name << "\n";
			closest = &obj;
			closestDistance = distance;
		}
	});

	if (closest != nullptr)
	{
		closest->onInteract(scene, *this);
		return;
	}

	// Using the Artifact with nothing in reach is a Practice run
	//if (artifact && artifact->isEnabled) { scene.StartMiniGame(artifactMode); return; }
}