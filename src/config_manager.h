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
const char* server_scheme();
const char* server_host();
uint16_t server_port();

/// Audio settings: compile-time defaults, then GET /config / hello (#29).
/// Leftover ``sample_rate`` / ``max_record_secs`` in config.json are ignored.
uint32_t sample_rate();
uint32_t max_record_secs();

/// Apply server audio fields. Zero keeps the current value. Out-of-range is ignored.
void apply_audio(uint32_t sample_rate, uint32_t max_record_secs);

/// Active GPIO pins from config.json "pins" array.
/// Room targets come from hello ``buttons``; these pins are hardware-only.
const uint8_t* active_pins();
uint8_t active_pin_count();

} // namespace ConfigManager
