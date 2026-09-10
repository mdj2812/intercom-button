#pragma once
#include "consts.hpp"
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

struct AudioSettings {
    uint32_t sample_rate;
    uint32_t max_record_secs;

    AudioSettings(uint32_t rate = AUDIO_SAMPLE_RATE_DEFAULT, uint32_t secs = AUDIO_MAX_RECORD_SECS_DEFAULT)
        : sample_rate(rate), max_record_secs(secs) {}
};

/// Merge server fields into current. Zero or out-of-range keeps the current value.
AudioSettings merge_audio(AudioSettings current, uint32_t sample_rate, uint32_t max_record_secs);

/// One blocking GET. Caller owns retry/backoff.
Result fetch(const char* server_scheme, const char* server_host, uint16_t server_port);

} // namespace ServerConfig
