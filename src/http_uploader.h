#pragma once
#include <cstddef>
#include <cstdint>

/// HTTP POST WAV data to the Home Intercom server.
namespace HTTPUploader {

/// POST WAV audio to /api/home_intercom/device/record.
/// Always identifies the device with X-Device-ID (STA MAC). No HA token.
/// When server_scheme is https, uses TLS (without cert verification).
/// Returns true if the server replied with HTTP 200 + {"ok":true},
/// OR if retries are exhausted on a transient error (audio delivery is
/// treated as best-effort). Auth failures (401/403) return false immediately.
bool upload(const uint8_t* data, size_t size, const char* server_scheme, const char* server_host, uint16_t server_port,
            const char* room_target, const char* device_id);

} // namespace HTTPUploader
