#pragma once
#include <cstdint>

/// Runtime configuration loaded from LittleFS /config.json.
/// Call ConfigManager::begin() once in setup(), then use the accessors.
namespace ConfigManager {

/// Mount LittleFS and load config. Returns true on success.
/// Falls back to hardcoded defaults if file is missing or invalid.
bool begin();

// ── Accessors ────────────────────────────────────────
const char* wifi_ssid();
const char* wifi_password();
const char* server_host();
uint16_t    server_port();
const char* room_target();

// Audio settings (read-only after boot)
uint32_t sample_rate();
uint32_t max_record_secs();

} // namespace ConfigManager
