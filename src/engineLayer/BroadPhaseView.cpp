#include "BroadPhaseView.h"

#include <ArenaAllocator.h>
#include <gameMap.h>

std::uint8_t BroadPhaseView::FlagsFor(const GameObject& object) noexcept
{
	const RigidBody3D& body = object.rigidBody3D;
	std::uint8_t flags = 0;
	if (body.collider.canCollide)  { flags |= BPF_CAN_COLLIDE; }
	if (body.collider.isTrigger()) { flags |= BPF_TRIGGER; }
	if (body.isStatic)             { flags |= BPF_STATIC; }
	return flags;
}

void BroadPhaseView::RefreshBox(std::size_t i) noexcept
{
	records[i].box = owners[i]->rigidBody3D.broadPhaseBox;
}

bool BroadPhaseView::Build(GameMap& map, Arena& arena)
{
	records = nullptr;
	owners = nullptr;
	total = 0;

	counts[BPG_OBJECT]       = map.GameObjectCount();
	counts[BPG_ENTITY]       = map.EntityCount();
	counts[BPG_INTERACTABLE] = map.InteractableCount();

	offsets[BPG_OBJECT] = 0;
	for (int g = 1; g < BPG_COUNT; ++g) { offsets[g] = offsets[g - 1] + counts[g - 1]; }

	const std::size_t n = offsets[BPG_COUNT - 1] + counts[BPG_COUNT - 1];
	if (n == 0) { return true; }   // an empty world is a valid, empty view

	// try_allocate, not allocate: a frame that cannot fit the view falls back to
	// walking the containers rather than throwing out of the solver.
	void* recMem = arena.try_allocate(n * sizeof(BroadPhaseRec), alignof(BroadPhaseRec));
	if (recMem == nullptr) { return false; }
	void* ownMem = arena.try_allocate(n * sizeof(GameObject*), alignof(GameObject*));
	if (ownMem == nullptr) { return false; }

	records = static_cast<BroadPhaseRec*>(recMem);
	owners = static_cast<GameObject**>(ownMem);
	total = n;

	// Filled in the order the ForEach helpers walk, so the pair sequence the
	// solver sees is the one it saw before this type existed.
	std::size_t at = offsets[BPG_OBJECT];
	map.ForEachGameObject([&](GameObject& object)
	{
		records[at].box = object.rigidBody3D.broadPhaseBox;
		records[at].flags = FlagsFor(object);
		owners[at] = &object;
		++at;
	});

	at = offsets[BPG_ENTITY];
	map.ForEachEntity([&](Entity& entity)
	{
		records[at].box = entity.rigidBody3D.broadPhaseBox;
		records[at].flags = FlagsFor(entity);
		owners[at] = &entity;
		++at;
	});

	at = offsets[BPG_INTERACTABLE];
	map.ForEachInteractable([&](InteractableObject& interactable)
	{
		records[at].box = interactable.rigidBody3D.broadPhaseBox;
		records[at].flags = FlagsFor(interactable);
		owners[at] = &interactable;
		++at;
	});

	return true;
}
