#pragma once
#ifndef WORLD_WORLD_KEY_H
#define WORLD_WORLD_KEY_H

// Which world a session is in (MultiplayerSplitPlan.md W-8, W-9, Q60).
//
//   world key = world_id + ":" + the World save's content hash (16 hex digits)
//
// The Host opens its World store under the key, so an edited World save opens a
// fresh world. A joining client sends the hash in Hello, so a client with a
// different World save is refused (WorldMismatch).

#include <cstdint>
#include <string>
#include <string_view>

namespace SceneManagement { struct Scene; }

namespace World {

	// ^[a-z0-9_]{1,32}$, checked when the config loads; the Host refuses to start
	// on anything else (W-8)
	bool IsValidWorldId(std::string_view worldId);

	// A valid world_id made from a World save's name: lower case, anything else
	// as '_', at most 32 characters, "world" for an empty name
	std::string WorldIdFromName(std::string_view worldName);

	// "<world_id>:<16 lower-case hex digits>"
	std::string MakeWorldKey(std::string_view worldId, std::uint64_t contentHash);

	// The key's shape, as the veil_world module checks it
	bool IsValidWorldKey(std::string_view worldKey);

	/**
	 * The one shared content hash (W-9): 64-bit FNV-1a over a fixed list of the
	 * scene's static fields, in id order, read straight after the World save
	 * loads and before any tick:
	 *   - the Task stations (the StationTable): id, game, Target id, minimum seconds
	 *   - the Exits: id and lock id
	 *   - every static collider: id, transform, collider shape and size
	 *   - every stalker's patrol waypoints
	 *   - the spawn point
	 * Floats are hashed by their bits, with -0 as +0. Runtime fields (a station
	 * saved as completed, a stalker's state or velocity) are left out, so they
	 * never change the key. Never raw file bytes, never std::hash.
	 */
	std::uint64_t ContentHash(const SceneManagement::Scene& scene);

	// FNV-1a, 64-bit, fed field by field
	class Fnv1a64
	{
	public:
		void Bytes(const void* data, std::size_t size);
		void U64(std::uint64_t value);
		void U32(std::uint32_t value);
		void I32(std::int32_t value) { U32(static_cast<std::uint32_t>(value)); }
		void Float(float value);
		std::uint64_t Value() const { return hash; }

	private:
		std::uint64_t hash = 0xcbf29ce484222325ull;
	};
}

#endif
