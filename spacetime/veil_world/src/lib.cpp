// veil_world: the World store for a Veil Host (MultiplayerSplitPlan.md §5.2,
// decisions Q38, Q50, Q51, Q60).
//
// Holds World state only: which Task stations in a world are completed. A row
// in station_state existing means that station is completed. Targets are never
// stored; the game derives them.
//
// Built and published by hand, outside the game's CMake (Q51). Shares no header
// with the game: the boundary is covered by the JSON round-trip test on the
// game side (W5). Never contains game code or GNS (§2.4).
//
// Both tables are public: the identity a Host mints with POST /v1/identity is
// not the database owner, so it can't read private tables. Only reducers write,
// and every reducer is idempotent and checks that the caller owns the world.

#include <spacetimedb.h>

#include <string>
#include <vector>

using namespace SpacetimeDB;

// One row per world. world_key is "<world_id>:<16 hex digits of content hash>",
// so an edited World save opens a fresh world (Q60).
struct World {
    std::string world_key;
    Identity owner;
};
SPACETIMEDB_STRUCT(World, world_key, owner);
SPACETIMEDB_TABLE(World, world, Public);
FIELD_PrimaryKey(world, world_key);

// One row per completed station. `key` is "<world_key>/<station_id>", which
// makes completing a station twice the same insert (idempotent).
struct StationState {
    std::string key;
    std::string world_key;
    uint64_t station_id;
};
SPACETIMEDB_STRUCT(StationState, key, world_key, station_id);
SPACETIMEDB_TABLE(StationState, station_state, Public);
FIELD_PrimaryKey(station_state, key);
FIELD_Index(station_state, world_key);

namespace {
    bool IsLowerAlnumOrUnderscore(char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    }

    bool IsLowerHex(char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    }

    // ^[a-z0-9_]{1,32}:[0-9a-f]{16}$, the same rule the game checks (W-8)
    bool IsValidWorldKey(const std::string& key) {
        const std::size_t colon = key.find(':');
        if (colon == std::string::npos || colon == 0 || colon > 32) { return false; }
        if (key.size() != colon + 1 + 16) { return false; }
        for (std::size_t i = 0; i < colon; ++i) {
            if (!IsLowerAlnumOrUnderscore(key[i])) { return false; }
        }
        for (std::size_t i = colon + 1; i < key.size(); ++i) {
            if (!IsLowerHex(key[i])) { return false; }
        }
        return true;
    }

    std::string StationKey(const std::string& world_key, uint64_t station_id) {
        return world_key + "/" + std::to_string(station_id);
    }
}

// Ensures the world exists and belongs to the caller. A missing world is
// created with the caller as its owner. Calling it again changes nothing.
SPACETIMEDB_REDUCER(open_world, ReducerContext ctx, std::string world_key) {
    if (!IsValidWorldKey(world_key)) {
        return Err("invalid world key");
    }

    if (auto existing = ctx.db[world_world_key].find(world_key)) {
        if (existing->owner != ctx.sender()) {
            return Err("not the owner of this world");
        }
        return Ok();
    }

    ctx.db[world].insert(World{ world_key, ctx.sender() });
    return Ok();
}

// Marks a station completed. Completing it again changes nothing.
SPACETIMEDB_REDUCER(complete_station, ReducerContext ctx, std::string world_key, uint64_t station_id) {
    if (!IsValidWorldKey(world_key)) {
        return Err("invalid world key");
    }

    auto owned = ctx.db[world_world_key].find(world_key);
    if (!owned) {
        return Err("no such world");
    }
    if (owned->owner != ctx.sender()) {
        return Err("not the owner of this world");
    }

    const std::string key = StationKey(world_key, station_id);
    if (ctx.db[station_state_key].find(key)) {
        return Ok();
    }

    ctx.db[station_state].insert(StationState{ key, world_key, station_id });
    return Ok();
}

// A Full wipe: deletes every completed station of the world. Wiping an empty
// world changes nothing.
SPACETIMEDB_REDUCER(reset_world, ReducerContext ctx, std::string world_key) {
    if (!IsValidWorldKey(world_key)) {
        return Err("invalid world key");
    }

    auto owned = ctx.db[world_world_key].find(world_key);
    if (!owned) {
        return Err("no such world");
    }
    if (owned->owner != ctx.sender()) {
        return Err("not the owner of this world");
    }

    // Collected first: deleting while the index is being walked is not allowed
    std::vector<std::string> keys;
    for (const auto& row : ctx.db[station_state_world_key].filter(world_key)) {
        keys.push_back(row.key);
    }
    for (const auto& key : keys) {
        ctx.db[station_state_key].delete_by_key(key);
    }
    return Ok();
}
