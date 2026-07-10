/**
 * @file ota_manager.cpp
 * @brief Over-the-air firmware update implementation.
 */

#include "ota_manager.h"

#include "config_manager.h"
#include "wifi_manager.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Update.h>

#include <cstdio>
#include <cstring>

#include <esp_ota_ops.h>
#include <Preferences.h>

// ── mbedTLS SHA-256 ──────────────────────────────────
#include <mbedtls/sha256.h>

#include "ota_crypto.h"
#include "ota_keys.h"

namespace OTAManager {

// ── Internal state ───────────────────────────────────────

static Progress s_progress;
static char s_firmware_path[128] = "/esp32-intercom-button.bin";
static bool s_update_pending = false;

// ── Public API ───────────────────────────────────────────

bool begin() {
    s_progress = Progress{};
    s_update_pending = false;

    Serial.println("[ota] Manager initialized");
    Serial.printf("[ota] Firmware URL: %s:%u%s\n", ConfigManager::server_host(), ConfigManager::server_port(),
                  s_firmware_path);
    return true;
}

Progress progress() {
    return s_progress;
}

bool update_pending() {
    return s_update_pending;
}

bool download_and_flash() {
    s_progress = Progress{};
    s_update_pending = false;

    // Grab the target OTA partition BEFORE Update.begin() claims it
    const esp_partition_t* target_part = esp_ota_get_next_update_partition(nullptr);
    if (!target_part) {
        s_progress.state = State::FAILED;
        s_progress.error = "No OTA partition available";
        Serial.println("[ota] FAILED: No OTA partition found");
        return false;
    }
    Serial.printf("[ota] Target partition: %s (0x%lx)\n", target_part->label, target_part->address);

    const char* host = ConfigManager::server_host();
    uint16_t port = ConfigManager::server_port();

    if (!WiFiManager::update()) {
        s_progress.state = State::FAILED;
        s_progress.error = "WiFi not connected";
        Serial.println("[ota] FAILED: WiFi not connected");
        return false;
    }

    // ── Download signature first (tiny, 64 bytes) ──────────
    uint8_t sig_buf[64] = {0};
    bool have_signature = false;
    {
        WiFiClient sig_client;
        sig_client.setTimeout(5000);
        if (sig_client.connect(host, port)) {
            String sig_path = String(s_firmware_path) + ".sig";
            sig_client.printf("GET %s HTTP/1.0\r\nHost: %s:%u\r\nConnection: close\r\n\r\n", sig_path.c_str(), host,
                              port);

            // Skip headers
            String hdr_line;
            int sig_content_len = -1;
            while (sig_client.connected() || sig_client.available()) {
                hdr_line = sig_client.readStringUntil('\n');
                hdr_line.trim();
                if (hdr_line.length() == 0)
                    break;
                if (hdr_line.startsWith("Content-Length:")) {
                    String cl = hdr_line.substring(15);
                    cl.trim();
                    sig_content_len = cl.toInt();
                }
            }

            if (sig_content_len == 64) {
                // Read exactly 64 bytes — server with HTTP/1.0 closes quickly
                size_t total = 0;
                unsigned long read_start = millis();
                while (total < 64 && (millis() - read_start) < 3000) {
                    int r = sig_client.read(sig_buf + total, 64 - total);
                    if (r > 0)
                        total += r;
                    else
                        delay(5);
                }
                if (total == 64) {
                    have_signature = true;
                    Serial.println("[ota] Signature downloaded (64 bytes)");
                }
            }
            sig_client.stop();
        }
        if (!have_signature) {
            Serial.println("[ota] No signature file found — proceeding without ECDSA verification");
        }
    }

    // ── Connect ───────────────────────────────────────────
    WiFiClient client;
    client.setTimeout(30000); // 30s for headers

    Serial.printf("[ota] Connecting to %s:%u...\n", host, port);
    if (!client.connect(host, port)) {
        s_progress.state = State::FAILED;
        s_progress.error = "Failed to connect to server";
        Serial.printf("[ota] FAILED: Could not connect to %s:%u\n", host, port);
        return false;
    }

    // ── HTTP GET request ──────────────────────────────────
    client.printf("GET %s HTTP/1.0\r\n", s_firmware_path);
    client.printf("Host: %s:%u\r\n", host, port);
    client.print("Connection: close\r\n");
    client.print("\r\n");

    // ── Read response headers ─────────────────────────────
    String line;
    int content_length = -1;
    char expected_sha_hex[65] = {0};

    unsigned long header_start = millis();
    while (client.connected() || client.available()) {
        line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            break; // empty line = end of headers

        if (line.startsWith("HTTP/")) {
            int http_code = line.substring(9, 12).toInt();
            if (http_code != 200) {
                s_progress.state = State::FAILED;
                s_progress.error = "HTTP error";
                Serial.printf("[ota] FAILED: HTTP %d\n", http_code);
                client.stop();
                return false;
            }
        } else if (line.startsWith("Content-Length:")) {
            String len_str = line.substring(15);
            len_str.trim();
            content_length = len_str.toInt();
        } else if (line.startsWith("X-Checksum-SHA256:")) {
            String sha_str = line.substring(19);
            sha_str.trim();
            strncpy(expected_sha_hex, sha_str.c_str(), 64);
            expected_sha_hex[64] = '\0';
        }

        if (millis() - header_start > 5000) {
            s_progress.state = State::FAILED;
            s_progress.error = "Header timeout";
            Serial.println("[ota] FAILED: Header read timeout");
            client.stop();
            return false;
        }
    }

    if (content_length <= 0) {
        s_progress.state = State::FAILED;
        s_progress.error = "Missing Content-Length";
        Serial.println("[ota] FAILED: Missing Content-Length header");
        client.stop();
        return false;
    }

    Serial.printf("[ota] Firmware size: %d bytes (%.1f KB)\n", content_length, content_length / 1024.0f);

    // ── Check space ───────────────────────────────────────
    if (!Update.begin(content_length)) {
        s_progress.state = State::FAILED;
        s_progress.error = "Update.begin() failed";
        Serial.printf("[ota] FAILED: Update.begin(%d) failed: %s\n", content_length, Update.errorString());
        client.stop();
        return false;
    }

    // ── SHA-256 context ───────────────────────────────────
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0); // 0 = SHA-256

    // ── Stream download → SHA-256 → flash ─────────────────
    s_progress.total_bytes = content_length;
    s_progress.state = State::DOWNLOADING;

    uint8_t buf[4096];
    unsigned long downloaded = 0;
    unsigned long last_report_ms = 0;

    while (client.connected() && downloaded < (unsigned long) content_length) {
        int avail = client.available();
        if (avail > 0) {
            int to_read = (avail > (int) sizeof(buf)) ? (int) sizeof(buf) : avail;
            int r = client.read(buf, to_read);
            if (r > 0) {
                // Hash stream
                mbedtls_sha256_update_ret(&sha, buf, r);
                // Write to OTA partition
                Update.write(buf, r);
                downloaded += r;
                s_progress.bytes_downloaded = downloaded;
                s_progress.percent = (int) (downloaded * 100 / content_length);

                // Progress report every 2 seconds
                if (millis() - last_report_ms > 2000) {
                    Serial.printf("[ota] Downloading... %d%% (%lu/%d)\n", s_progress.percent, downloaded,
                                  content_length);
                    last_report_ms = millis();
                }
            }
        }
        delay(1);
    }

    // Drain any remaining
    while (client.available()) {
        int r = client.read(buf, (int) sizeof(buf));
        if (r > 0) {
            mbedtls_sha256_update_ret(&sha, buf, r);
            Update.write(buf, r);
            downloaded += r;
            s_progress.bytes_downloaded = downloaded;
        }
    }

    client.stop();

    Serial.printf("[ota] Downloaded %lu bytes\n", downloaded);

    if (downloaded != (unsigned long) content_length) {
        s_progress.state = State::FAILED;
        s_progress.error = "Incomplete download";
        Serial.printf("[ota] FAILED: Expected %d bytes, got %lu\n", content_length, downloaded);
        Update.abort();
        mbedtls_sha256_free(&sha);
        return false;
    }

    // ── SHA-256 verification ──────────────────────────────
    s_progress.state = State::VERIFYING;
    uint8_t computed_hash[32];
    mbedtls_sha256_finish_ret(&sha, computed_hash);
    mbedtls_sha256_free(&sha);

    char computed_hex[65];
    OTACrypto::hexify(computed_hash, computed_hex);
    Serial.printf("[ota] SHA-256: %s\n", computed_hex);

    if (expected_sha_hex[0] != '\0') {
        // Server provided expected checksum — verify
        uint8_t expected_hash[32];
        if (!OTACrypto::unhexify(expected_sha_hex, expected_hash)) {
            s_progress.state = State::FAILED;
            s_progress.error = "Bad checksum header format";
            Serial.printf("[ota] FAILED: Could not parse checksum header '%s'\n", expected_sha_hex);
            Update.abort();
            return false;
        }

        if (memcmp(computed_hash, expected_hash, 32) != 0) {
            char expected_hex[65];
            OTACrypto::hexify(expected_hash, expected_hex);
            s_progress.state = State::FAILED;
            s_progress.error = "SHA-256 mismatch";
            Serial.printf("[ota] FAILED: SHA-256 mismatch\n  got:      %s\n  expected: %s\n", computed_hex,
                          expected_hex);
            Update.abort();
            return false;
        }
        Serial.println("[ota] SHA-256 verified OK");
    } else {
        // No checksum from server — log warning but proceed
        Serial.println("[ota] WARNING: No X-Checksum-SHA256 header — skipping verification");
    }

    // ── ECDSA signature verification ───────────────────────
    if (have_signature) {
        Serial.println("[ota] Verifying ECDSA signature...");

        if (!OTACrypto::verify_ecdsa_signature(computed_hash, sig_buf, OTA_PUBLIC_KEY, OTA_PUBLIC_KEY_LEN)) {
            s_progress.state = State::FAILED;
            s_progress.error = "ECDSA: signature verification failed";
            Serial.println("[ota] FAILED: ECDSA signature invalid");
            Update.abort();
            return false;
        }

        Serial.println("[ota] ECDSA signature verified OK");
    }

    // ── Finalize flash ────────────────────────────────────
    if (!Update.end()) {
        s_progress.state = State::FAILED;
        s_progress.error = Update.errorString();
        Serial.printf("[ota] FAILED: Update.end() — %s\n", Update.errorString());
        return false;
    }

    // Set boot partition AFTER flash is complete (partition must have valid content)
    if (esp_ota_set_boot_partition(target_part) != ESP_OK) {
        s_progress.state = State::FAILED;
        s_progress.error = "Failed to set boot partition";
        Serial.println("[ota] FAILED: Could not set boot partition");
        return false;
    }
    Serial.printf("[ota] Boot partition → %s\n", target_part->label);

    // Set NVS flag: boot confirmation needed on next boot
    {
        Preferences prefs;
        if (prefs.begin("ota", false)) {
            prefs.putBool("pending", true);
            prefs.end();
        }
    }

    s_progress.state = State::SUCCESS;
    s_progress.percent = 100;
    s_update_pending = true;

    Serial.println("[ota] === SUCCESS === Firmware written to inactive OTA slot");
    Serial.println("[ota] REBOOT to apply the update.");

    return true;
}

// ── Rollback / Boot Confirmation ────────────────────────

bool is_pending_verify() {
    Preferences prefs;
    if (!prefs.begin("ota", true))
        return false;
    bool pending = prefs.getBool("pending", false);
    prefs.end();
    return pending;
}

void confirm_boot() {
    esp_ota_mark_app_valid_cancel_rollback();

    // Reset NVS flags
    Preferences prefs;
    if (prefs.begin("ota", false)) {
        prefs.putBool("pending", false);
        prefs.putUInt("fail_count", 0);
        prefs.end();
    }

    Serial.println("[ota] Boot confirmed — image marked valid");
}

int boot_failure_count() {
    Preferences prefs;
    if (!prefs.begin("ota", true))
        return 0;
    unsigned int count = prefs.getUInt("fail_count", 0);
    prefs.end();
    return (int) count;
}

static void increment_failure_count() {
    Preferences prefs;
    if (prefs.begin("ota", false)) {
        unsigned int count = prefs.getUInt("fail_count", 0) + 1;
        prefs.putUInt("fail_count", count);
        prefs.end();
        Serial.printf("[ota] Boot failure count: %u/%d\n", count, MAX_BOOT_FAILURES);
    }
}

void increment_failure() {
    increment_failure_count();
}

void mark_invalid_and_rollback() {
    Serial.println("[ota] Marking image as invalid — rollback on next boot");
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

} // namespace OTAManager
