/**
 * ESP32-S3 Desktop Intercom Button — Multi-Button Edition
 *
 * Push-to-talk buttons that record audio via MAX9814 electret mic,
 * send WAV to Flask intercom server, which broadcasts to Home Assistant speakers.
 *
 * Each button is mapped to a target room via NVS (RoomTargetStore).
 * After hello, GET /api/home_intercom/rooms fills that map (pin index → room key).
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
#include <Preferences.h>
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
#include "room_fetcher.h"
#include "room_target_store.h"
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
static unsigned long MAX_RECORD_MS = 60000; // updated from config

static uint8_t active_button_index = 0; // which button triggered recording
static unsigned long upload_start_ms = 0;
static unsigned long confirm_deadline_ms = 0; // boot confirmation timeout

static bool hello_ok = false;
static bool was_wifi_ok = false;
static unsigned long next_hello_ms = 0;
static unsigned long hello_backoff_ms = 2000;
static unsigned long pending_since_ms = 0;
static const unsigned long HELLO_BACKOFF_MAX_MS = 60000;

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
                             DeviceId::mac(), FIRMWARE_VERSION);
}

static void refresh_rooms_from_server() {
    RoomFetcher::Result rooms =
        RoomFetcher::fetch(ConfigManager::server_scheme(), ConfigManager::server_host(), ConfigManager::server_port());
    if (!rooms.ok) {
        Serial.printf("[main] Room fetch failed (%s) — keeping last NVS/config map\n",
                      rooms.error ? rooms.error : "error");
        return;
    }
    if (rooms.count == 0) {
        Serial.println("[main] Server room catalog empty — keeping local map");
        return;
    }
    uint8_t written = RoomFetcher::apply(room_store, active_pins, active_pin_count, rooms);
    Serial.printf("[main] Room map from server (%u keys, %u written)\n", rooms.count, written);
    for (uint8_t i = 0; i < active_pin_count; i++) {
        if (i < rooms.count)
            Serial.printf("[main] GPIO%u → %s\n", active_pins[i], rooms.keys[i]);
        else
            Serial.printf("[main] GPIO%u → %s (local, no server key)\n", active_pins[i],
                          room_store.get_room(active_pins[i]).c_str());
    }
}

static void on_hello_ok(const DeviceHello::Result& hello, bool fetch_rooms) {
    hello_ok = true;
    hello_backoff_ms = 2000;
    clear_pending_wait();
    next_hello_ms = millis() + HELLO_HEARTBEAT_MS;
    if (hello.max_record_secs > 0) {
        MAX_RECORD_MS = hello.max_record_secs * 1000UL;
    }
    if (fetch_rooms)
        refresh_rooms_from_server();
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
    MAX_RECORD_MS = ConfigManager::max_record_secs() * 1000UL;

    Serial.printf("Server: %s://%s:%u | Device: %s | Max: %us\n", ConfigManager::server_scheme(),
                  ConfigManager::server_host(), ConfigManager::server_port(), DeviceId::mac(),
                  ConfigManager::max_record_secs());

    // ── Per-button rooms: NVS (filled by GET /rooms after hello) ─
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
    Serial.println(" (targets from GET /rooms after hello)");

    // ── Button manager ──────────────────────────────
    buttons.begin(active_pins, active_pin_count);

    // ── WiFi ────────────────────────────────────────
    WiFiManager::begin(ConfigManager::wifi_ssid(), ConfigManager::wifi_password());

    // ── Audio recorder ──────────────────────────────
    if (!recorder.begin(ConfigManager::sample_rate(), ConfigManager::max_record_secs())) {
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
        Serial.printf("[main] Confirm boot within %lu seconds (press any button)\n", OTAManager::CONFIRM_TIMEOUT_SEC);
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
        } else if (cmd.length() > 0) {
            Serial.printf("[main] Unknown command: '%s' (try 'ota' or 'reboot')\n", cmd.c_str());
        }
    }

    switch (state) {

        case State::IDLE: {
            if (wifi_ok && !was_wifi_ok) {
                next_hello_ms = 0;
                hello_backoff_ms = 2000;
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

                on_hello_ok(hello, true);
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
                    on_hello_ok(hello, false);
                    Serial.println("[main] Hello heartbeat OK");
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
            // Blink orange — fast blink for urgency
            led_blink(C_ORANGE, 300);

            // Button press = user confirms boot OK
            if (event.type == ButtonManager::EventType::PRESS) {
                OTAManager::confirm_boot();
                led_blink_n(C_GREEN, 1, 250);
                Serial.println("[main] Boot confirmed — normal operation");
                state = State::IDLE;
                break;
            }

            // Serial command confirm
            if (Serial.available()) {
                String cmd = Serial.readStringUntil('\n');
                cmd.trim();
                if (cmd == "confirm") {
                    OTAManager::confirm_boot();
                    led_blink_n(C_GREEN, 1, 250);
                    Serial.println("[main] Boot confirmed via serial");
                    state = State::IDLE;
                    break;
                }
            }

            // Timeout → rollback
            if (millis() > confirm_deadline_ms) {
                Serial.println("[main] Boot confirmation timeout — rolling back");
                OTAManager::increment_failure();

                int fail_count = OTAManager::boot_failure_count();
                if (fail_count >= OTAManager::MAX_BOOT_FAILURES) {
                    OTAManager::mark_invalid_and_rollback();
                }
                // Otherwise just restart — bootloader will retry same partition
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
                // Failure — blink red 3x, return to IDLE
                Serial.printf("[main] OTA failed: %s\n", OTAManager::progress().error);
                led_blink_n(C_RED, 3, 200);
                state = State::IDLE;
            }
            break;
        }

    } // switch

    delay(10);
}
