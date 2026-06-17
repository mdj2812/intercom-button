#pragma once
#include <cstdint>

/// Shared constants for esp32-intercom-button.
/// Single source of truth for magic numbers used across multiple modules.

// Maximum number of physical buttons supported
constexpr uint8_t MAX_BUTTONS = 8;

// Maximum length of a room key string (e.g. "study", "living")
constexpr uint8_t MAX_ROOM_KEY_LEN = 32;
