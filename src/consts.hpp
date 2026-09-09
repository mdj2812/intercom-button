#pragma once
#include <cstdint>

/// Shared constants for esp32-intercom-button.
/// Single source of truth for magic numbers used across multiple modules.

// Maximum number of physical buttons supported
constexpr uint8_t MAX_BUTTONS = 8;

// Maximum length of a room key string (e.g. "study", "living")
constexpr uint8_t MAX_ROOM_KEY_LEN = 32;

// Maximum length of a server-assigned device name from hello
constexpr uint8_t MAX_DEVICE_NAME_LEN = 64;

/// Reported to the server in POST /devices/hello.
constexpr const char* FIRMWARE_VERSION = "0.1.0";

/// Re-hello while idle so HA last_seen stays inside the 5-minute online window.
constexpr unsigned long HELLO_HEARTBEAT_MS = 10000;

/// Transient heartbeat failure (timeout / 5xx): stay registered, retry sooner.
constexpr unsigned long HELLO_HEARTBEAT_RETRY_MS = 30000;

/// While hello is pending approval: retry this often for HELLO_PENDING_BURST_MS.
constexpr unsigned long HELLO_PENDING_RETRY_MS = 200;

/// How long to poll aggressively after the first pending (then HELLO_PENDING_SLOW_MS).
constexpr unsigned long HELLO_PENDING_BURST_MS = 30000;

/// After the burst, keep waiting without using the 60s error cap.
constexpr unsigned long HELLO_PENDING_SLOW_MS = 5000;
