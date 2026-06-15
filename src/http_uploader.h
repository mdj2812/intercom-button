#pragma once
#include <cstdint>
#include <cstddef>

/// HTTP POST WAV data to Flask intercom server /convert endpoint.
namespace HTTPUploader {

/// POST WAV audio to server. Blocks until HTTP response received.
/// Returns true if server replied with HTTP 200 + {"ok":true}.
bool upload(const uint8_t *data, size_t size,
            const char *server_host, uint16_t server_port,
            const char *room_target);

} // namespace HTTPUploader
