#include <GameObject.h>

#include <Scene.h>
#include <gameMap.h>
#include <Player.h>
#include <sim/Presentation.h>


// Interacting with a Task station is a claim on it (rule S-3). The server's
// StationTable decides; the MiniGame screen opens on the client that gets the
// grant (Client::PresentEvent). The unlock gate and the activation noise are
// the claim's.
void ActivateMiniGame(SceneManagement::Scene& scene, InteractableObject* interactable, Player& by)
{
	std::cout << "[InteractableObject] Claiming station " << interactable->id << " (MiniGame " << interactable->variation << ")\n";
	scene.ClaimStation(scene.StationIndexOf(interactable->id), Sim::SlotForPlayerId(by.id));
}

GameObject* InteractableObject::FindMiniGameTarget(GameMap& gameMap)
{
	if (targetId == 0) { return this; }
	return gameMap.FindWorldObject(targetId);
}

bool InteractableObject::IsExitUnlocked(const GameMap& gameMap) const
{
	if (interactType != INTERACT_EXIT || !isEnabled) { return false; }

	if (targetId != 0)
	{
		// The lock is a Target like any door: open once disabled, or deleted
		const GameObject* lock = nullptr;
		gameMap.ForEachGameObject([&](const GameObject& object) { if (object.id == targetId) { lock = &object; } });
		gameMap.ForEachInteractable([&](const InteractableObject& object) { if (object.id == targetId) { lock = &object; } });
		gameMap.ForEachEntity([&](const Entity& object) { if (object.id == targetId) { lock = &object; } });
		return lock == nullptr || !lock->isEnabled;
	}

	// No lock named: the task is every Task station
	bool anyStation = false;
	bool allDone = true;
	gameMap.ForEachInteractable([&](const InteractableObject& station) {
		if (station.interactType != INTERACT_MINIGAME) { return; }
		anyStation = true;
		allDone = allDone && station.isCompleted;
	});
	return anyStation && allDone;
}

void Unlock(SceneManagement::Scene& /*scene*/, InteractableObject* interactable, Player& by)
{
	std::cout << "[InteractableObject] Unlocking MiniGame: " << interactable->variation << "\n";
	auto player = &by;
	player->artifactUnlocked = std::max(player->artifactUnlocked, interactable->variation);
	player->artifactUnlocked = Clamp(player->artifactUnlocked, MINI_GAME_FLAPPY_BIRD_ID, MINI_GAME_RO_SHAM_BOO_ID);

	// Check If Player Has Artifact
	if (!player->artifact) {
		std::cout << "Create Artifact \n";
		// Create Artifact
		player->artifact.emplace();
		player->artifact->name = "Artifact";
		player->artifact->isDestructible = false;
		player->artifact->rigidBody3D.scale = Vector3One();
		player->artifact->rigidBody3D.collider.canCollide = false;
		player->artifact->rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f);
		player->artifact->rigidBody3D.angularVelocity = Vector3(0.0f, 1.0f, 0.0f);
		player->artifact->rigidBody3D.lockAngularVelocity = true;
		player->artifact->defaultSound = "Artifact_Load_Up";

		// Set Model Based On Upgrade
		std::string model = "RubixCube" + std::to_string(player->artifactUnlocked);
		player->artifact->setModel(model);
	}
	else {
		// Set Model Based On Upgrade
		std::string model = "RubixCube" + std::to_string(player->artifactUnlocked);
		player->artifact->setModel(model);
	}

	interactable->isInteractable = false;

	// Removed at the end of the frame by GameMap::SweepDestroyed, not here: this
	// runs inside the station's own onInteract
	interactable->Destroy();
}

InteractableObject::InteractableObject(const InteractionType interact, int value)
{
	type = OBJECT_INTERACTABLE;
	interactType = interact; // Type Of Interaction
	activator = value; // Item
}
InteractableObject::InteractableObject(const InteractionType interact, int variant, int activate )
{
	type = OBJECT_INTERACTABLE;
	interactType = interact; // Type of interaction
	variation = variant; // Minigame
	activator = activate; // Object
}

void InteractableObject::onInteract(SceneManagement::Scene& scene, Player& by)
{
	Sim::CurrentPresentation().PlayObjectSound(*this);
	std::cout << "[InteractObject] Interacted with " << name << "\n";

	// The branches stay mutually exclusive: Unlock() marks this station for
	// removal, and none of the others may run on a station that is going away.
	const InteractionType type = interactType;
	if (type == INTERACT_MINIGAME) { ActivateMiniGame(scene, this, by); }
	else if (type == INTERACT_UNLOCK) { Unlock(scene, this, by); }

}

Json InteractableObject::formatToJson()
{
	Json j;
	addCommonToJson(j);
	j["InteractType"] = interactType;
	j["Variation"] = variation;
	j["Activator"] = activator;
	j["TargetId"] = targetId;
	j["IsInteractable"] = isInteractable;
	j["IsCompleted"] = isCompleted;
	j["MinimumSeconds"] = minimumSeconds;
	// isRunningMiniGame is deliberately not persisted: it means "occupied right
	// now", and a save reloaded later has nobody standing at the station.
	return j;
}

bool InteractableObject::loadFromJson(Json& j)
{
	if (!loadCommonFromJson(j)) { return false; }
	interactType = static_cast<InteractionType>(j.value("InteractType", 0));
	variation = j.value("Variation", 0);
	// 0 is the "no activator" sentinel every other path uses — both constructors
	// default to it and ActivateMiniGame tests against it. Defaulting to -1 made
	// a save written before ActivatorValue existed load as a real-but-
	// unresolvable id instead of "none".
	activator = j.value("Activator", 0);

	// Saves written before a station had a Target of its own kept it in Activator
	if (j.contains("TargetId")) {
		targetId = j.value("TargetId", std::uint64_t{ 0 });
	}
	else if (interactType == INTERACT_MINIGAME) {
		targetId = static_cast<std::uint64_t>(std::max(activator, 0));
		activator = 0;
	}

	isInteractable = true;
	isCompleted = j.value("IsCompleted", false);
	minimumSeconds = j.value("MinimumSeconds", kDefaultMinimumSeconds);
	if (!(minimumSeconds >= 0.0f)) { minimumSeconds = kDefaultMinimumSeconds; } // also rejects NaN

	// A station is never occupied on load, whatever was happening when the save
	// was written — the same reason the Stalker drops its lastKnownPosition.
	isRunningMiniGame = false;
	return true;
}
