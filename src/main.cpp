/**
 * ESP32-S3 Desktop Intercom Button — Multi-Button Edition
 *
 * Push-to-talk buttons that record audio via MAX9814 electret mic,
 * send WAV to Flask intercom server, which broadcasts to Home Assistant speakers.
 *
 * Each button is mapped to a target room via NVS (RoomTargetStore).
 * After hello (boot and idle heartbeat), a non-empty ``buttons`` map is applied
 * (home-intercom#78 / #39). Empty ``{}`` or a missing field keeps last NVS.
 * Pins omitted from a non-empty map are unassigned (home-intercom#74).
 * GET /config supplies sample_rate and max_record_secs (#29); hello repeats them.
 * GET /media_players is the PWA speaker catalog, not a room map.
 * Configuration is loaded from LittleFS /config.json at boot.
 *
 * Hardware:
 *   - ESP32-S3-DevKitC
 *   - MAX9814 mic module (VCC→3.3V, GND→GND, OUT→GPIO1)
 *   - Push buttons (GPIO {4,5,12,13}→GND, active low, internal pull-up)
 */

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <Adafruit_NeoPixel.h>
#include <cstdio>
#include <Preferences.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "audio_recorder.h"
#include "button_manager.h"
#include "config.h"
#include "config_manager.h"
#include "consts.hpp"
#include "device_hello.h"
#include "device_id.h"
#include "http_uploader.h"
#include "ota_manager.h"
#include "room_target_store.h"
#include "server_config.h"
#include "wifi_manager.h"

// ── Globals ─────────────────────────────────────────
static ButtonManager buttons;
static RoomTargetStore room_store;
static AudioRecorder recorder;
static Adafruit_NeoPixel led(1, PIN_LED_WS2812, NEO_GRB + NEO_KHZ800);

static const uint8_t* active_pins = BUTTON_PINS;
static uint8_t active_pin_count = BUTTON_COUNT;

enum class State { IDLE, RECORDING, UPLOADING, CONFIRMING, OTA };
static State state = State::IDLE;
static unsigned long record_start_ms = 0;
static const unsigned long MIN_RECORD_MS = 500;
static uint32_t audio_sample_rate = AUDIO_SAMPLE_RATE_DEFAULT;
static uint32_t audio_max_secs = AUDIO_MAX_RECORD_SECS_DEFAULT;
static unsigned long MAX_RECORD_MS = AUDIO_MAX_RECORD_SECS_DEFAULT * 1000UL;
static unsigned long ota_skip_until_ms = 0;

static uint8_t active_button_index = 0; // which button triggered recording
static unsigned long upload_start_ms = 0;
static unsigned long confirm_deadline_ms = 0; // boot confirmation timeout

static bool hello_ok = false;
static bool was_wifi_ok = false;
static unsigned long next_hello_ms = 0;
static unsigned long hello_backoff_ms = 2000;
static unsigned long pending_since_ms = 0;
static const unsigned long HELLO_BACKOFF_MAX_MS = 60000;
static bool confirm_dry_run = false;
static bool confirm_skip_hello = false;
static char last_room_fp[192] = {};
static bool server_audio_ok = false;
static unsigned long next_config_ms = 0;

static unsigned long pending_retry_delay() {
    if (pending_since_ms == 0)
        pending_since_ms = millis();
    if (millis() - pending_since_ms < HELLO_PENDING_BURST_MS)
        return HELLO_PENDING_RETRY_MS;
    return HELLO_PENDING_SLOW_MS;
}

static void clear_pending_wait() {
    pending_since_ms = 0;
}

static DeviceHello::Result send_hello() {
    return DeviceHello::send(ConfigManager::server_scheme(), ConfigManager::server_host(), ConfigManager::server_port(),
                             DeviceId::mac(), FIRMWARE_VERSION, active_pins, active_pin_count);
}

static void log_gpio_rooms() {
    for (uint8_t i = 0; i < active_pin_count; i++) {
        std::string room = room_store.get_room(active_pins[i]);
        if (room.empty())
            Serial.printf("[main] GPIO%u → (unassigned)\n", active_pins[i]);
        else
            Serial.printf("[main] GPIO%u → %s\n", active_pins[i], room.c_str());
    }
}

static void room_map_fingerprint(char* out, size_t n) {
    size_t used = 0;
    if (!out || n == 0)
        return;
    out[0] = '\0';
    for (uint8_t i = 0; i < active_pin_count && used + 1 < n; i++) {
        std::string room = room_store.get_room(active_pins[i]);
        int w = snprintf(out + used, n - used, "%u=%s;", active_pins[i], room.c_str());
        if (w < 0)
            break;
        used += static_cast<size_t>(w);
        if (used >= n) {
            out[n - 1] = '\0';
            break;
        }
    }
}

static void note_room_map(const char* source, uint8_t keys, uint8_t written) {
    char fp[192];
    room_map_fingerprint(fp, sizeof(fp));
    if (strcmp(fp, last_room_fp) == 0)
        return;
    strncpy(last_room_fp, fp, sizeof(last_room_fp) - 1);
    last_room_fp[sizeof(last_room_fp) - 1] = '\0';
    Serial.printf("[main] Room map from %s (%u keys, %u written)\n", source, keys, written);
    log_gpio_rooms();
}

static void apply_hello_buttons(const DeviceHello::Result& hello) {
    uint8_t written = 0;
    for (uint8_t p = 0; p < active_pin_count; p++) {
        uint8_t pin = active_pins[p];
        const char* room = nullptr;
        for (uint8_t i = 0; i < hello.button_count; i++) {
            if (hello.button_gpios[i] == pin) {
                room = hello.button_rooms[i];
                break;
            }
        }
        if (room && room[0] != '\0') {
            if (room_store.set_room(pin, room))
                written++;
        } else {
            room_store.set_room(pin, "");
        }
    }
    note_room_map("hello", hello.button_count, written);
}

static void finish_boot_confirm(const char* via) {
    if (!confirm_dry_run)
        OTAManager::confirm_boot();
    Serial.printf("[main] Boot confirmed via %s%s\n", via, confirm_dry_run ? " (dry-run)" : "");
    confirm_dry_run = false;
    confirm_skip_hello = false;
}

static void begin_confirm_dry_run(bool skip_hello) {
    confirm_dry_run = true;
    confirm_skip_hello = skip_hello;
    next_hello_ms = 0;
    state = State::CONFIRMING;
    confirm_deadline_ms = millis() + OTAManager::CONFIRM_DRY_RUN_SEC * 1000UL;
    Serial.printf("[main] OTA confirm dry-run — %lu s (%s, button, or serial confirm); timeout returns to idle\n",
                  OTAManager::CONFIRM_DRY_RUN_SEC, skip_hello ? "hello skipped" : "hello");
}

static void apply_server_audio(uint32_t rate, uint32_t secs) {
    ServerConfig::AudioSettings next = ServerConfig::merge_audio({audio_sample_rate, audio_max_secs}, rate, secs);
    MAX_RECORD_MS = next.max_record_secs * 1000UL;
    if (next.sample_rate == audio_sample_rate && next.max_record_secs == audio_max_secs)
        return;
    if (!recorder.configure(next.sample_rate, next.max_record_secs)) {
        MAX_RECORD_MS = audio_max_secs * 1000UL;
        Serial.printf("[main] Audio reconfigure failed — keeping %u Hz / %us\n", audio_sample_rate, audio_max_secs);
        return;
    }
    audio_sample_rate = next.sample_rate;
    audio_max_secs = next.max_record_secs;
    Serial.printf("[main] Audio from server: %u Hz, max %us\n", audio_sample_rate, audio_max_secs);
}

static void fetch_server_audio_if_due() {
    if (server_audio_ok || millis() < next_config_ms)
        return;
    ServerConfig::Result cfg =
        ServerConfig::fetch(ConfigManager::server_scheme(), ConfigManager::server_host(), ConfigManager::server_port());
    if (!cfg.ok) {
        next_config_ms = millis() + AUDIO_CONFIG_RETRY_MS;
        Serial.printf("[main] GET /config failed (%s) — retry in %lu ms\n", cfg.error ? cfg.error : "error",
                      AUDIO_CONFIG_RETRY_MS);
        return;
    }
    apply_server_audio(cfg.sample_rate, cfg.max_record_secs);
    server_audio_ok = true;
}

static void on_hello_ok(const DeviceHello::Result& hello) {
    hello_ok = true;
    hello_backoff_ms = 2000;
    clear_pending_wait();
    next_hello_ms = millis() + HELLO_HEARTBEAT_MS;
    if (hello.sample_rate > 0 || hello.max_record_secs > 0)
        apply_server_audio(hello.sample_rate, hello.max_record_secs);
    if (hello.has_buttons)
        apply_hello_buttons(hello);
}

static bool should_start_ota(const DeviceHello::Result& hello) {
    if (!hello.ota)
        return false;
    if (OTAManager::is_pending_verify())
        return false;
    if ((long) (millis() - ota_skip_until_ms) < 0)
        return false;
    return true;
}

// ── LED color constants ────────────────────────────────

struct LED {
    uint8_t r, g, b;
};
constexpr LED C_OFF{0, 0, 0};
constexpr LED C_GREEN{0, 255, 0};
constexpr LED C_RED{255, 0, 0};
constexpr LED C_BLUE{0, 0, 255};
constexpr LED C_ORANGE{255, 128, 0};
constexpr LED C_WHITE{255, 255, 255};

// ── LED helpers ───────────────────────────────────────

static void led_set(const LED& c) {
    led.setPixelColor(0, led.Color(c.r, c.g, c.b));
    led.show();
}

static void led_blink_n(const LED& c, int count, int interval_ms) {
    for (int i = 0; i < count; i++) {
        led_set(c);
        delay(interval_ms);
        led_set(C_OFF);
        delay(interval_ms);
    }
}

static void led_blink(const LED& c, unsigned long period_ms) {
    bool on = (millis() / (period_ms / 2)) % 2 == 0;
    led_set(on ? c : C_OFF);
}

/// Orange pairing blink runs on a side task so it continues while hello HTTP blocks.
static volatile bool led_pairing_blink = false;

static void led_pairing_task(void*) {
    for (;;) {
        if (led_pairing_blink) {
            bool on = (millis() / 400) % 2 == 0;
            if (on)
                led.setPixelColor(0, led.Color(C_ORANGE.r, C_ORANGE.g, C_ORANGE.b));
            else
                led.setPixelColor(0, 0);
            led.show();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ── SETUP ───────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== ESP32-S3 Intercom Button (Multi) ===");

    led.begin();
    led_set(C_OFF);
    led.setBrightness(32);
    xTaskCreatePinnedToCore(led_pairing_task, "led_pair", 2048, nullptr, 1, nullptr, 0);

    // ── Load runtime config from LittleFS ───────────
    ConfigManager::begin();
    MAX_RECORD_MS = audio_max_secs * 1000UL;

    Serial.printf("Server: %s://%s:%u | Device: %s | Audio default: %u Hz / %us\n", ConfigManager::server_scheme(),
                  ConfigManager::server_host(), ConfigManager::server_port(), DeviceId::mac(), audio_sample_rate,
                  audio_max_secs);

    // ── Per-button rooms: NVS, filled from hello buttons ─
    if (!room_store.begin()) {
        Serial.println("[main] NVS init failed — using defaults");
    }

    if (ConfigManager::active_pin_count() > 0) {
        active_pins = ConfigManager::active_pins();
        active_pin_count = ConfigManager::active_pin_count();
    }

    Serial.printf("[main] %u buttons:", active_pin_count);
    for (uint8_t i = 0; i < active_pin_count; i++)
        Serial.printf(" GPIO%u", active_pins[i]);
    Serial.println(" (targets from hello buttons)");

    // ── Button manager ──────────────────────────────
    buttons.begin(active_pins, active_pin_count);

    // ── WiFi ────────────────────────────────────────
    WiFiManager::begin(ConfigManager::wifi_ssid(), ConfigManager::wifi_password());

    // ── Audio recorder ──────────────────────────────
    if (!recorder.begin(audio_sample_rate, audio_max_secs)) {
        Serial.println("FATAL: AudioRecorder init failed");
        while (1) {
            led_blink(C_RED, 200);
            delay(10);
        }
    }

    // ── OTA manager ─────────────────────────────────
    OTAManager::begin();

    // ── Boot confirmation (after OTA reboot) ────────
    {
        const esp_partition_t* running = esp_ota_get_running_partition();
        if (running && running->subtype == 0) { // factory subtype
            Preferences prefs;
            if (prefs.begin("ota", false)) {
                prefs.putBool("pending", false);
                prefs.putUInt("fail_count", 0);
                prefs.end();
                Serial.println("[main] Factory boot — cleared stale OTA flags");
            }
        }
    }
    if (OTAManager::is_pending_verify()) {
        int fail_count = OTAManager::boot_failure_count();
        Serial.printf("[main] OTA boot — pending verification (failures: %d/%d)\n", fail_count,
                      OTAManager::MAX_BOOT_FAILURES);

        if (fail_count >= OTAManager::MAX_BOOT_FAILURES) {
            Serial.println("[main] Too many failures — marking image invalid");
            OTAManager::mark_invalid_and_rollback();
            // mark_invalid_and_rollback() calls reboot internally
            return;
        }

        state = State::CONFIRMING;
        confirm_deadline_ms = millis() + OTAManager::CONFIRM_TIMEOUT_SEC * 1000UL;
        Serial.printf("[main] Confirm boot within %lu seconds (hello, button, or serial confirm)\n",
                      OTAManager::CONFIRM_TIMEOUT_SEC);
    }

    Serial.println("Setup complete — ready.");
}

// ── LOOP ────────────────────────────────────────────

void loop() {
    bool wifi_ok = WiFiManager::update();
    auto event = buttons.poll();

    // ── Serial commands (skip in CONFIRMING/OTA — they have their own handlers)
    if (Serial.available() && state != State::CONFIRMING && state != State::OTA) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "ota" && state == State::IDLE && wifi_ok) {
            Serial.println("[main] OTA update triggered via serial");
            state = State::OTA;
        } else if (cmd == "reboot") {
            Serial.println("[main] Rebooting...");
            delay(100);
            ESP.restart();
        } else if (cmd == "confirm_test" || cmd.startsWith("confirm_test ")) {
            if (state != State::IDLE) {
                Serial.println("[main] confirm_test only from idle");
            } else {
                String arg;
                if (cmd.startsWith("confirm_test "))
                    arg = cmd.substring(13);
                arg.trim();
                if (arg.length() > 0 && arg != "nohello") {
                    Serial.printf(
                        "[main] Unknown confirm_test arg: '%s' (try 'confirm_test' or 'confirm_test nohello')\n",
                        arg.c_str());
                } else {
                    begin_confirm_dry_run(arg == "nohello");
                }
            }
        } else if (cmd.length() > 0) {
            Serial.printf("[main] Unknown command: '%s' (try 'ota', 'reboot', or 'confirm_test')\n", cmd.c_str());
        }
    }

    switch (state) {

        case State::IDLE: {
            if (wifi_ok && !was_wifi_ok) {
                next_hello_ms = 0;
                hello_backoff_ms = 2000;
                next_config_ms = 0;
                server_audio_ok = false;
            }
            if (!wifi_ok) {
                hello_ok = false;
                clear_pending_wait();
            }
            was_wifi_ok = wifi_ok;

            if (!wifi_ok) {
                led_pairing_blink = false;
                led_blink(C_RED, 500);
                break;
            }

            fetch_server_audio_if_due();

            if (!hello_ok) {
                led_pairing_blink = true;
                if (millis() < next_hello_ms)
                    break;

                DeviceHello::Result hello = send_hello();

                if (hello.status != DeviceHello::Status::Ok) {
                    unsigned long backoff = hello_backoff_ms;
                    if (hello.status == DeviceHello::Status::Revoked) {
                        backoff = HELLO_REVOKED_RETRY_MS;
                    } else if (hello.status == DeviceHello::Status::Pending) {
                        backoff = pending_retry_delay();
                    }
                    next_hello_ms = millis() + backoff;
                    if (hello.status != DeviceHello::Status::Pending) {
                        if (hello_backoff_ms < HELLO_BACKOFF_MAX_MS)
                            hello_backoff_ms = hello_backoff_ms * 2;
                        if (hello_backoff_ms > HELLO_BACKOFF_MAX_MS)
                            hello_backoff_ms = HELLO_BACKOFF_MAX_MS;
                    }
                    if (hello.status == DeviceHello::Status::Pending) {
                        Serial.printf("[main] Waiting for approval — retry in %lu ms\n", backoff);
                    } else {
                        Serial.printf("[main] Hello failed (%s) — retry in %lu ms\n",
                                      hello.error ? hello.error : "error", backoff);
                    }
                    break;
                }

                on_hello_ok(hello);
                if (should_start_ota(hello)) {
                    Serial.println("[main] OTA requested by server");
                    state = State::OTA;
                    break;
                }
            }

            led_pairing_blink = false;
            led_set(C_GREEN);

            // PTT wins over a due heartbeat — do not delay talk with a hello POST.
            // Single-threaded loop(): no race on active_button_index —
            // ButtonManager::poll() runs synchronously, ISR only touches
            // volatile flags inside ButtonManager.
            if (event.type == ButtonManager::EventType::PRESS) {
                active_button_index = event.button_index;
                recorder.start();
                record_start_ms = millis();
                led_set(C_BLUE);
                state = State::RECORDING;
                Serial.printf("[main] Recording for GPIO%u...\n", active_pins[active_button_index]);
                break;
            }

            if (millis() >= next_hello_ms) {
                DeviceHello::Result hello = send_hello();
                if (hello.status == DeviceHello::Status::Ok) {
                    on_hello_ok(hello);
                    Serial.println("[main] Hello heartbeat OK");
                    if (should_start_ota(hello)) {
                        Serial.println("[main] OTA requested by server");
                        state = State::OTA;
                        break;
                    }
                } else if (hello.status == DeviceHello::Status::Revoked) {
                    hello_ok = false;
                    hello_backoff_ms = HELLO_REVOKED_RETRY_MS;
                    next_hello_ms = millis() + HELLO_REVOKED_RETRY_MS;
                    Serial.printf("[main] Hello heartbeat: %s — will re-register\n",
                                  hello.error ? hello.error : "blocked");
                } else if (hello.status == DeviceHello::Status::Pending) {
                    hello_ok = false;
                    unsigned long delay_ms = pending_retry_delay();
                    next_hello_ms = millis() + delay_ms;
                    Serial.printf("[main] Hello heartbeat: waiting for approval — retry in %lu ms\n", delay_ms);
                } else {
                    next_hello_ms = millis() + HELLO_HEARTBEAT_RETRY_MS;
                    Serial.printf("[main] Hello heartbeat failed (%s) — retry in %lu ms\n",
                                  hello.error ? hello.error : "error", HELLO_HEARTBEAT_RETRY_MS);
                }
            }
            break;
        }

        case State::RECORDING: {
            led_set(C_BLUE);
            unsigned long elapsed = millis() - record_start_ms;

            if (elapsed >= MAX_RECORD_MS) {
                Serial.println("[main] Max time reached — stopping");
                goto stop_and_upload;
            }

            // Ignore presses on other buttons while recording
            if (event.type == ButtonManager::EventType::PRESS && event.button_index != active_button_index) {
                Serial.printf("[main] Busy — recording GPIO%u, ignoring GPIO%u\n", active_pins[active_button_index],
                              active_pins[event.button_index]);
                led_set(C_RED);
                delay(150);
                break;
            }

            if (event.type == ButtonManager::EventType::RELEASE && event.button_index == active_button_index) {
                Serial.printf("[main] Released after %lu ms\n", elapsed);
            stop_and_upload:
                recorder.stop();

                if (elapsed < MIN_RECORD_MS) {
                    Serial.println("[main] Too short — discarding");
                    led_set(C_GREEN);
                    state = State::IDLE;
                    break;
                }

                recorder.write_wav_header();
                upload_start_ms = millis();
                state = State::UPLOADING;
                Serial.println("[main] Uploading...");
            }
            break;
        }

        case State::UPLOADING: {
            led_blink(C_WHITE, 100);

            uint8_t gpio = active_pins[active_button_index];
            std::string room = room_store.get_room(gpio);

            if (room.empty()) {
                Serial.printf("[main] GPIO%u unassigned — skip upload\n", gpio);
                led_blink_n(C_RED, 4, 125);
                state = State::IDLE;
                break;
            }

            bool ok = HTTPUploader::upload(recorder.data(), recorder.total_bytes(), ConfigManager::server_scheme(),
                                           ConfigManager::server_host(), ConfigManager::server_port(), room.c_str(),
                                           DeviceId::mac());

            unsigned long upload_ms = millis() - upload_start_ms;
            Serial.printf("[main] Upload to %s %s (%lu ms)\n", room.c_str(), ok ? "OK" : "FAILED", upload_ms);

            led_blink_n(ok ? C_GREEN : C_RED, 4, 125);

            state = State::IDLE;
            break;
        }

        case State::CONFIRMING: {
            was_wifi_ok = wifi_ok;
            led_blink(C_ORANGE, 300);

            if (event.type == ButtonManager::EventType::PRESS) {
                finish_boot_confirm("button");
                led_blink_n(C_GREEN, 1, 250);
                state = State::IDLE;
                break;
            }

            if (Serial.available()) {
                String cmd = Serial.readStringUntil('\n');
                cmd.trim();
                if (cmd == "confirm") {
                    finish_boot_confirm("serial");
                    led_blink_n(C_GREEN, 1, 250);
                    state = State::IDLE;
                    break;
                }
            }

            // Hello proves the new image can reach Home Intercom. Do not start
            // another OTA from this response (should_start_ota already skips
            // pending-verify; after confirm the server should have cleared ota).
            if (!confirm_skip_hello && wifi_ok && millis() >= next_hello_ms) {
                DeviceHello::Result hello = send_hello();
                if (DeviceHello::confirms_ota_boot(hello.status)) {
                    finish_boot_confirm("hello");
                    if (hello.status == DeviceHello::Status::Ok) {
                        on_hello_ok(hello);
                        led_blink_n(C_GREEN, 1, 250);
                    } else if (hello.status == DeviceHello::Status::Revoked) {
                        hello_ok = false;
                        hello_backoff_ms = HELLO_REVOKED_RETRY_MS;
                        next_hello_ms = millis() + HELLO_REVOKED_RETRY_MS;
                    } else {
                        hello_ok = false;
                        unsigned long delay_ms = pending_retry_delay();
                        next_hello_ms = millis() + delay_ms;
                    }
                    state = State::IDLE;
                    break;
                }
                next_hello_ms = millis() + HELLO_OTA_CONFIRM_RETRY_MS;
                Serial.printf("[main] Hello failed (%s) — retry in %lu ms\n", hello.error ? hello.error : "error",
                              HELLO_OTA_CONFIRM_RETRY_MS);
            }

            if (millis() > confirm_deadline_ms) {
                if (confirm_dry_run) {
                    Serial.println("[main] Boot confirmation timeout (dry-run) — returning to idle");
                    confirm_dry_run = false;
                    confirm_skip_hello = false;
                    led_set(C_RED);
                    delay(250);
                    state = State::IDLE;
                    break;
                }
                Serial.println("[main] Boot confirmation timeout — rolling back");
                OTAManager::increment_failure();

                int fail_count = OTAManager::boot_failure_count();
                if (fail_count >= OTAManager::MAX_BOOT_FAILURES) {
                    OTAManager::mark_invalid_and_rollback();
                }
                led_set(C_RED);
                delay(1000);
                ESP.restart();
            }
            break;
        }

        case State::OTA: {
            led_set(C_ORANGE); // orange = OTA in progress

            bool ok = OTAManager::download_and_flash();

            if (ok) {
                // Success — blink green 3x then reboot
                led_blink_n(C_GREEN, 3, 200);
                Serial.println("[main] OTA success — rebooting...");
                delay(500);
                ESP.restart();
            } else {
                // Failure — blink red 3x, skip hello-ota for a few minutes
                Serial.printf("[main] OTA failed: %s\n", OTAManager::progress().error);
                led_blink_n(C_RED, 3, 200);
                ota_skip_until_ms = millis() + OTA_RETRY_SKIP_MS;
                state = State::IDLE;
            }
            break;
        }

    } // switch

    delay(10);
}
