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
    /// Hello JSON included a ``buttons`` object (home-intercom#78). Empty ``{}``
    /// is unconfigured: keep last NVS. Missing field: same (no catalog fallback).
    bool buttons_field = false;
    /// Non-empty parsed GPIO → room map. Apply this; omitted pins are unassigned.
    bool has_buttons = false;
    uint8_t button_count = 0;
    uint8_t button_gpios[MAX_BUTTONS] = {};
    char button_rooms[MAX_BUTTONS][MAX_ROOM_KEY_LEN] = {};
};

/// Parsed hello (ok / pending / revoked) proves the new image can talk to Home Intercom.
/// Transport, HTTP, and JSON failures do not — keep the OTA confirm deadline.
inline bool confirms_ota_boot(Status status) {
    return status == Status::Ok || status == Status::Pending || status == Status::Revoked;
}

/// One blocking POST. Caller owns retry/backoff.
/// ``pins`` is advertised so the PWA can bind GPIOs using GET /media_players (#81).
Result send(const char* server_scheme, const char* server_host, uint16_t server_port, const char* device_id,
            const char* firmware_version, const uint8_t* pins = nullptr, uint8_t pin_count = 0);

} // namespace DeviceHello
