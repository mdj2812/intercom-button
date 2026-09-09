#pragma once
#include "consts.hpp"
#include <cstdint>

/// Trust-on-first-use registration: POST /api/home_intercom/devices/hello.
namespace DeviceHello {

enum class Status {
    Ok,
    Pending, // server pairing gate (issue #36); not registered for record yet
    Revoked,
    Error,
};

struct Result {
    Status status = Status::Error;
    const char* error = nullptr;
    char device_name[MAX_DEVICE_NAME_LEN] = {};
    char room[MAX_ROOM_KEY_LEN] = {};
    uint32_t sample_rate = 0;     // 0 = not provided
    uint32_t max_record_secs = 0; // 0 = not provided
    bool ota = false;             // server wants this device to flash LAN firmware
};

/// One blocking POST. Caller owns retry/backoff.
Result send(const char* server_scheme, const char* server_host, uint16_t server_port, const char* device_id,
            const char* firmware_version);

} // namespace DeviceHello
