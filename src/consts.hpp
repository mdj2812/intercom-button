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

/// HTTP timeout for POST /devices/hello. Must exceed the server pending-hello
/// hold (HOME_INTERCOM_PENDING_HELLO_WAIT, default 8s) so approve can return ok.
constexpr uint32_t HELLO_HTTP_TIMEOUT_MS = 30000;

/// After status=pending, retry immediately so a hello is usually in-flight
/// when the admin approves (home-intercom #51 / issue #36).
constexpr unsigned long HELLO_PENDING_RETRY_MS = 200;
