#include "WorldKey.h"

#include <Scene.h>
#include <AI/Stalker.h>

#include <algorithm>
#include <bit>
#include <cstdio>
#include <vector>

namespace World {

	bool IsValidWorldId(std::string_view worldId)
	{
		if (worldId.empty() || worldId.size() > 32) { return false; }
		return std::all_of(worldId.begin(), worldId.end(), [](char c) {
			return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
		});
	}

	std::string WorldIdFromName(std::string_view worldName)
	{
		std::string id;
		for (const char c : worldName)
		{
			if (id.size() == 32) { break; }
			if (c >= 'A' && c <= 'Z') { id.push_back(static_cast<char>(c - 'A' + 'a')); }
			else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') { id.push_back(c); }
			else { id.push_back('_'); }
		}
		return id.empty() ? std::string("world") : id;
	}

	std::string MakeWorldKey(std::string_view worldId, std::uint64_t contentHash)
	{
		char hex[17] = {};
		std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(contentHash));
		std::string key(worldId);
		key.push_back(':');
		key.append(hex, 16);
		return key;
	}

	bool IsValidWorldKey(std::string_view worldKey)
	{
		const std::size_t colon = worldKey.find(':');
		if (colon == std::string_view::npos) { return false; }
		if (!IsValidWorldId(worldKey.substr(0, colon))) { return false; }
		const std::string_view hash = worldKey.substr(colon + 1);
		return hash.size() == 16 && std::all_of(hash.begin(), hash.end(), [](char c) {
			return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
		});
	}

	void Fnv1a64::Bytes(const void* data, std::size_t size)
	{
		const auto* bytes = static_cast<const unsigned char*>(data);
		for (std::size_t i = 0; i < size; ++i)
		{
			hash ^= bytes[i];
			hash *= 0x100000001b3ull;
		}
	}

	void Fnv1a64::U64(std::uint64_t value)
	{
		// Little-endian byte order, whatever the machine's, so the hash is the
		// same on every platform
		unsigned char bytes[8];
		for (int i = 0; i < 8; ++i) { bytes[i] = static_cast<unsigned char>(value >> (8 * i)); }
		Bytes(bytes, sizeof(bytes));
	}

	void Fnv1a64::U32(std::uint32_t value)
	{
		unsigned char bytes[4];
		for (int i = 0; i < 4; ++i) { bytes[i] = static_cast<unsigned char>(value >> (8 * i)); }
		Bytes(bytes, sizeof(bytes));
	}

	void Fnv1a64::Float(float value)
	{
		if (value == 0.0f) { value = 0.0f; } // -0 hashes as +0
		U32(std::bit_cast<std::uint32_t>(value));
	}

	namespace {
		void HashVector(Fnv1a64& h, Vector3 v) { h.Float(v.x); h.Float(v.y); h.Float(v.z); }

		// Distinct section markers, so moving data between sections changes the hash
		enum Section : std::uint32_t { kStations = 1, kExits = 2, kColliders = 3, kWaypoints = 4, kSpawn = 5 };
	}

	std::uint64_t ContentHash(const SceneManagement::Scene& scene)
	{
		Fnv1a64 h;
		const GameMap& map = scene.gameMap;

		// Task stations and Exits, in id order
		std::vector<const InteractableObject*> stations;
		std::vector<const InteractableObject*> exits;
		map.ForEachInteractable([&](const InteractableObject& object) {
			if (object.interactType == INTERACT_MINIGAME) { stations.push_back(&object); }
			else if (object.interactType == INTERACT_EXIT) { exits.push_back(&object); }
		});
		const auto byId = [](const GameObject* a, const GameObject* b) { return a->id < b->id; };
		std::sort(stations.begin(), stations.end(), byId);
		std::sort(exits.begin(), exits.end(), byId);

		h.U32(kStations);
		h.U32(static_cast<std::uint32_t>(stations.size()));
		for (const InteractableObject* station : stations)
		{
			h.U64(station->id);
			h.I32(station->variation);
			h.U64(station->targetId);
			h.Float(station->minimumSeconds);
		}

		h.U32(kExits);
		h.U32(static_cast<std::uint32_t>(exits.size()));
		for (const InteractableObject* exit : exits)
		{
			h.U64(exit->id);
			h.U64(exit->targetId);
		}

		// Every static collider, in id order: what players and the stalker walk among
		std::vector<const GameObject*> colliders;
		const auto collectStatic = [&](const GameObject& object) {
			const auto& body = object.rigidBody3D;
			if (body.isStatic && body.collider.canCollide) { colliders.push_back(&object); }
		};
		map.ForEachGameObject(collectStatic);
		map.ForEachInteractable(collectStatic);
		map.ForEachEntity(collectStatic);
		std::sort(colliders.begin(), colliders.end(), byId);

		h.U32(kColliders);
		h.U32(static_cast<std::uint32_t>(colliders.size()));
		for (const GameObject* object : colliders)
		{
			const auto& body = object->rigidBody3D;
			h.U64(object->id);
			HashVector(h, body.translation);
			h.Float(body.rotation.x); h.Float(body.rotation.y); h.Float(body.rotation.z); h.Float(body.rotation.w);
			HashVector(h, body.scale);
			h.U32(static_cast<std::uint32_t>(body.collider.shape));
			h.U32(static_cast<std::uint32_t>(body.collider.mode));
			HashVector(h, body.collider.size);
			HashVector(h, body.collider.offset);
			h.Float(body.collider.radius);
		}

		// Patrol routes, by stalker id
		std::vector<const Stalker*> stalkers;
		map.ForEachStalker([&](const Stalker& stalker) { stalkers.push_back(&stalker); });
		std::sort(stalkers.begin(), stalkers.end(), [](const Stalker* a, const Stalker* b) { return a->id < b->id; });

		h.U32(kWaypoints);
		h.U32(static_cast<std::uint32_t>(stalkers.size()));
		for (const Stalker* stalker : stalkers)
		{
			h.U64(stalker->id);
			h.U32(static_cast<std::uint32_t>(stalker->waypoints.size()));
			for (const Vector3& point : stalker->waypoints) { HashVector(h, point); }
		}

		// Where players spawn (each Slot's offset from it is a constant of the code)
		h.U32(kSpawn);
		HashVector(h, scene.GetSpawnPoint());

		return h.Value();
	}
}
