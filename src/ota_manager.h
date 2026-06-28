#pragma once
/**
 * @file ota_manager.h
 * @brief Over-the-air firmware update module.
 *
 * Downloads firmware binary over HTTP, streams to the inactive OTA partition,
 * verifies SHA-256 checksum, and reports progress.
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
 */
bool begin();

/**
 * Download firmware from the configured URL and flash to the inactive
 * OTA partition. SHA-256 checksum is verified against the
 * X-Checksum-SHA256 response header.
 *
 * Blocks during download. Returns true if flash + verify succeeded.
 */
bool download_and_flash();

/** Get current progress snapshot. Thread-safe to call from loop(). */
Progress progress();

/** Convenience: returns true if an update was just completed successfully. */
bool update_pending();

} // namespace OTAManager
