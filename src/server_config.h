#pragma once
#include <cstdint>

/// GET /api/home_intercom/config — global audio settings (#29).
/// Public like /version: no device id, works before hello pairing.
namespace ServerConfig {

struct Result {
    bool ok = false;
    const char* error = nullptr;
    uint32_t sample_rate = 0;     // 0 = not provided
    uint32_t max_record_secs = 0; // 0 = not provided
};

/// One blocking GET. Caller owns retry/backoff.
Result fetch(const char* server_scheme, const char* server_host, uint16_t server_port);

} // namespace ServerConfig
