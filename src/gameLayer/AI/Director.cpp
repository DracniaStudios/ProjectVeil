#include <AI/Director.h>

#include <AI/Stalker.h>
#include <Scene.h>

#include <raymath.h>
#include <iostream>

void Director::Record(const DirectorHint& hint)
{
	if (log.size() >= kLogCapacity) { log.erase(log.begin()); }
	log.push_back(hint);

	// Printed as well as retained. The acceptance criterion is verification by
	// log, and the inspector panel is not always open when the interesting
	// hint fires.
	std::cout << "[Director] hint -> (" << hint.region.x << ", " << hint.region.z
	          << ") confidence " << hint.confidence
	          << " because: " << hint.reason << "\n";
}

void Director::Update(Scene* scene, float deltaTime)
{
	if (scene == nullptr) { return; }

	clock += deltaTime;
	if (cooldown > 0.0f) { cooldown -= deltaTime; }

	// Find a stalker to advise. The slice has exactly one; the loop is here so
	// that stops being an assumption baked into the Director.
	Stalker* stalker = nullptr;
	for (auto& [id, entity] : scene->gameMap.entities)
	{
		if (entity && entity->kind == ENTITYKIND_STALKER)
		{
			stalker = static_cast<Stalker*>(entity.get());
			break;
		}
	}
	if (stalker == nullptr) { return; }

	if (cooldown > 0.0f) { return; }

	// Fact 1: is the stalker actually idle? A hint during a hunt or an
	// investigation is noise at best and interference at worst, and Stalker
	// ignores hints while hunting anyway.
	if (stalker->state != STALKER_PATROL) { return; }

	// Fact 2: has it been quiet long enough that the stalker has nothing of
	// its own to act on?
	const float quietFor = scene->soundField.Now() - stalker->lastStimulusTime;
	if (quietFor < kQuietBeforeHint) { return; }

	// Fact 3: which objectives are outstanding?
	//
	// Deliberately skipping any station currently being used. That station is
	// where the player is standing, so hinting toward it would hand the AI a
	// live position under the cover of a "world fact". Unattended stations are
	// places the player must eventually go, which is pressure without
	// knowledge.
	const InteractableObject* target = nullptr;
	float bestDistance = 0.0f;
	for (auto& [id, interactable] : scene->gameMap.interactables)
	{
		if (!interactable) { continue; }
		if (interactable->interactType != INTERACT_MINIGAME) { continue; }
		if (interactable->isRunningMiniGame) { continue; }
		if (!interactable->isInteractable) { continue; }

		// Farthest outstanding station from the stalker: hinting toward the one
		// it is already near would be a no-op, and covering the far side of the
		// room is what actually forces re-traversal to be risky.
		const float distance = Vector3Distance(stalker->getPosition(), interactable->getPosition());
		if (target == nullptr || distance > bestDistance)
		{
			target = interactable.get();
			bestDistance = distance;
		}
	}

	if (target == nullptr) { return; }

	DirectorHint hint = {};
	hint.region = target->getPosition();
	hint.confidence = kFixedConfidence;
	hint.reason = "stalker idle and quiet; farthest unattended task station";
	hint.timestamp = clock;

	stalker->ApplyDirectorHint(hint.region, hint.confidence);
	Record(hint);

	cooldown = kHintCooldown;
}
