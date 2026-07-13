#pragma once
#include <cstddef>
#include <cstdint>

/// HTTP POST WAV data to Flask intercom server /record endpoint.
namespace HTTPUploader {

/// POST WAV audio to server. Blocks until HTTP response received.
/// When ha_token is non-empty, adds Authorization: Bearer header.
/// Returns true if server replied with HTTP 200 + {"ok":true},
/// OR if all retries exhausted (audio delivery is treated as best-effort).
bool upload(const uint8_t* data, size_t size, const char* server_host, uint16_t server_port, const char* room_target,
            const char* ha_token);

} // namespace HTTPUploader
