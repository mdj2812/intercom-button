#pragma once
#include "consts.hpp"
#include "room_target_store.h"
#include <cstdint>

/// GET /api/home_intercom/rooms — global room catalog (no auth).
/// Pin[i] is assigned catalog key[i] (document order). Extra pins are unassigned
/// so a PWA delete (#74) does not leave a stale NVS key.
namespace RoomFetcher {

struct Result {
    bool ok = false;
    const char* error = nullptr;
    uint8_t count = 0;
    char keys[MAX_BUTTONS][MAX_ROOM_KEY_LEN] = {};
};

/// One blocking GET. Does not send X-Device-ID (endpoint is public).
Result fetch(const char* server_scheme, const char* server_host, uint16_t server_port);

/// Write pin[i] → rooms.keys[i] into NVS. Pins past the catalog are unassigned.
/// Returns the number of mappings stored (not counting clears).
uint8_t apply(RoomTargetStore& store, const uint8_t* pins, uint8_t pin_count, const Result& rooms);

} // namespace RoomFetcher
