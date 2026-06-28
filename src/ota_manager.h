#pragma once
/**
 * @file ota_manager.h
 * @brief Over-the-air firmware update module.
 *
 * Downloads firmware binary over HTTP, streams to the inactive OTA partition,
 * verifies SHA-256 checksum + ECDSA signature, and manages rollback.
 */

#include <cstdint>

namespace OTAManager {

enum class State {
    IDLE,
    DOWNLOADING,
    VERIFYING,
    SUCCESS,
    FAILED,
};

struct Progress {
    State state = State::IDLE;
    unsigned long bytes_downloaded = 0;
    unsigned long total_bytes = 0;
    int percent = 0;
    const char* error = nullptr;
};

/**
 * Init the OTA manager. Must be called once in setup().
 * Reads OTA config from ConfigManager.
 * Checks boot state: if we just OTA'd, enters pending_verify mode.
 */
bool begin();

/**
 * Download firmware from the configured URL and flash to the inactive
 * OTA partition. SHA-256 checksum and ECDSA signature are verified.
 *
 * Blocks during download. Returns true if flash + verify succeeded.
 */
bool download_and_flash();

/** Get current progress snapshot. Thread-safe to call from loop(). */
Progress progress();

/** Convenience: returns true if an update was just completed successfully. */
bool update_pending();

// ── Rollback / boot confirmation ─────────────────────

/** True if current boot is from a pending-verify OTA slot. */
bool is_pending_verify();

/**
 * Confirm the current boot as successful.
 * Marks the OTA image as valid so bootloader won't roll back.
 * Resets the consecutive-failure counter in NVS.
 */
void confirm_boot();

/** Number of consecutive boot failures for the current image. */
int boot_failure_count();

/** Maximum allowed consecutive failures before permanently rejecting an image. */
static constexpr int MAX_BOOT_FAILURES = 3;

/** Seconds to wait for user confirmation before auto-rollback. */
static constexpr unsigned long CONFIRM_TIMEOUT_SEC = 60;

} // namespace OTAManager
