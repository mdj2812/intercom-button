#pragma once
#include <cstdint>

/// Shared constants for esp32-intercom-button.
/// Single source of truth for magic numbers used across multiple modules.

// Maximum number of physical buttons supported
constexpr uint8_t MAX_BUTTONS = 8;

// Maximum length of a room key string (e.g. "study", "living")
constexpr uint8_t MAX_ROOM_KEY_LEN = 32;

/// Reported to the server in POST /devices/hello.
constexpr const char* FIRMWARE_VERSION = "0.1.0";

/// Re-hello while idle so HA last_seen stays inside the 5-minute online window.
constexpr unsigned long HELLO_HEARTBEAT_MS = 120000;

/// Transient heartbeat failure (timeout / 5xx): stay registered, retry sooner.
constexpr unsigned long HELLO_HEARTBEAT_RETRY_MS = 30000;
