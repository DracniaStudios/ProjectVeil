/**
 * Tick digest harness (work order A-01; decision A-D9).
 *
 * One number that changes only if the simulation's result changes: the
 * before/after check for every structural work order. Test code only.
 *
 * The script and the hashed field list are fixed forever. Changing either
 * changes every recorded digest.
 */
#pragma once

#include <Scene.h>
#include <SaveSystem.h>
#include <AI/Stalker.h>
#include <World/WorldKey.h>   // World::Fnv1a64
#include <sim/FixedTick.h>
#include <sim/PlayerCommand.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

namespace TickDigest {

	// The scripted command for tick `tick` (0-based). Never completes a Task
	// station (a completion starts a wall-clock buff, H3).
	inline Sim::PlayerCommand CommandFor(int tick)
	{
		Sim::PlayerCommand c = {};
		c.sequence = static_cast<std::uint32_t>(tick + 1);
		c.clientTick = static_cast<std::uint32_t>(tick + 1);
		std::uint16_t buttons = 0;
		if (tick < 600)
		{
			c.moveY = 1;
			c.yawDelta = 0.004f;
		}
		else if (tick < 900)
		{
			c.moveY = 1;
			buttons |= Sim::BUTTON_SPRINT;
			c.yawDelta = -0.006f;
		}
		else if (tick < 1200)
		{
			c.moveX = 1;
		}
		else if (tick < 1800)
		{
			c.moveY = -1;
			buttons |= Sim::BUTTON_CROUCH;
			c.pitchDelta = (tick % 120 < 60) ? 0.002f : -0.002f;
		}
		else
		{
			c.moveY = 1;
			c.yawDelta = 0.01f;
		}
		if (tick % 240 == 120) buttons |= Sim::BUTTON_JUMP;
		if (tick == 1500 || tick == 2500) buttons |= Sim::BUTTON_INTERACT;
		if (tick == 2000) buttons |= Sim::BUTTON_FLASHLIGHT;
		c.buttons = buttons;
		return c;
	}

	namespace Detail {

		inline void HashVec(World::Fnv1a64& h, const Vector3& v)
		{
			h.Float(v.x); h.Float(v.y); h.Float(v.z);
		}

		inline void HashBool(World::Fnv1a64& h, bool b) { h.U32(b ? 1u : 0u); }

		inline void HashObject(World::Fnv1a64& h, GameObject& o)
		{
			h.U64(static_cast<std::uint64_t>(o.id));
			HashVec(h, o.rigidBody3D.translation);
			h.Float(o.rigidBody3D.rotation.x); h.Float(o.rigidBody3D.rotation.y);
			h.Float(o.rigidBody3D.rotation.z); h.Float(o.rigidBody3D.rotation.w);
			HashVec(h, o.getVelocity());
			HashVec(h, o.rigidBody3D.scale);
			HashBool(h, o.isEnabled);
			HashBool(h, o.pendingDestroy);
			HashVec(h, o.rigidBody3D.collider.size);
			HashVec(h, o.rigidBody3D.collider.offset);
		}
	}

	// FNV-1a over the simulated state, objects in ascending id order
	inline std::uint64_t Of(SceneManagement::Scene& scene)
	{
		World::Fnv1a64 h;

		// 1. The scene
		h.I32(static_cast<std::int32_t>(scene.simTick));
		h.I32(static_cast<std::int32_t>(scene.roundEpoch));
		Detail::HashBool(h, scene.roundWon);
		h.Float(scene.soundField.Now());
		h.Float(scene.director.Now());
		h.I32(static_cast<std::int32_t>(scene.soundField.Events().size()));

		// 2. Each player, in Slot order
		scene.ForEachPlayer([&](int slot, Player& p) {
			h.I32(slot);
			h.U64(static_cast<std::uint64_t>(p.id));
			Detail::HashVec(h, p.rigidBody3D.translation);
			Detail::HashVec(h, p.getVelocity());
			h.Float(p.rigidBody3D.rotation.x); h.Float(p.rigidBody3D.rotation.y);
			h.Float(p.rigidBody3D.rotation.z); h.Float(p.rigidBody3D.rotation.w);
			h.Float(p.lookYaw);
			h.Float(p.lookPitch);
			h.Float(p.health);
			h.Float(p.stamina);
			Detail::HashBool(h, p.isEnabled);
			Detail::HashBool(h, p.isCrouching);
			Detail::HashBool(h, p.isSprinting);
			Detail::HashBool(h, p.flashlightOn);
			h.I32(static_cast<std::int32_t>(p.artifactUnlocked));
		});

		// 3. Every world object from all three pools, sorted by id
		std::vector<GameObject*> objects;
		scene.gameMap.ForEachGameObject([&](GameObject& o) { objects.push_back(&o); });
		scene.gameMap.ForEachEntity([&](Entity& e) { objects.push_back(&e); });
		scene.gameMap.ForEachInteractable([&](InteractableObject& i) { objects.push_back(&i); });
		std::sort(objects.begin(), objects.end(), [](const GameObject* a, const GameObject* b) { return a->id < b->id; });
		for (GameObject* o : objects)
		{
			Detail::HashObject(h, *o);
			if (Entity* e = dynamic_cast<Entity*>(o))
			{
				h.Float(e->health);
				h.Float(e->stamina);
				if (e->kind == ENTITYKIND_STALKER)
				{
					Stalker& s = static_cast<Stalker&>(*e);
					h.I32(static_cast<std::int32_t>(s.state));
					h.Float(s.timeInState);
					Detail::HashBool(h, s.hasLastKnown);
					Detail::HashVec(h, s.lastKnownPosition);
					h.I32(s.currentWaypoint);
					Detail::HashVec(h, s.searchTarget);
				}
			}
		}

		// 4. Each Task station, in index order
		for (int i = 0; i < scene.stations.Count(); ++i)
		{
			const auto& st = scene.stations.At(i);
			h.U64(static_cast<std::uint64_t>(st.id));
			h.I32(st.holder);
			Detail::HashBool(h, st.completedInWorld);
		}

		return h.Value();
	}

	// Scene_new, SetSpawnPoint({20,2,0}), LoadWorld("chunk_1"), then `ticks`
	// scripted ticks; returns Of(scene) and deletes the scene. 0 if the load fails.
	inline std::uint64_t RunChunk1(int ticks)
	{
		SceneManagement::Scene* scene = SceneManagement::Scene_new();
		scene->SetSpawnPoint(Vector3{20, 2, 0});
		if (!SaveSystem::LoadWorld("chunk_1", *scene))
		{
			std::printf("  TickDigest: the World save chunk_1 failed to load\n");
			SceneManagement::Scene_delete(scene);
			return 0;
		}
		for (int tick = 0; tick < ticks; ++tick)
		{
			const Sim::PlayerCommand cmd = CommandFor(tick);
			SceneManagement::Scene_tick(*scene, Sim::kTickDelta, std::span<const Sim::PlayerCommand>(&cmd, 1));
		}
		const std::uint64_t digest = Of(*scene);
		SceneManagement::Scene_delete(scene);
		return digest;
	}
}
