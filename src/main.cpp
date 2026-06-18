/**
 * ESP32-S3 Desktop Intercom Button — Multi-Button Edition
 *
 * Push-to-talk buttons that record audio via MAX9814 electret mic,
 * send WAV to Flask intercom server, which broadcasts to Home Assistant speakers.
 *
 * Each button is mapped to a target room via NVS (RoomTargetStore).
 * Configuration is loaded from LittleFS /config.json at boot.
 *
 * Hardware:
 *   - ESP32-S3-DevKitC
 *   - MAX9814 mic module (VCC→3.3V, GND→GND, OUT→GPIO1)
 *   - Push buttons (GPIO {4,5,12,13}→GND, active low, internal pull-up)
 */

#include "audio_recorder.h"
#include "button_manager.h"
#include "config.h"
#include "config_manager.h"
#include "http_uploader.h"
#include "room_target_store.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ── Globals ─────────────────────────────────────────
static ButtonManager buttons;
static RoomTargetStore room_store;
static AudioRecorder recorder;
static Adafruit_NeoPixel led(1, PIN_LED_WS2812, NEO_GRB + NEO_KHZ800);

static const uint8_t* active_pins = BUTTON_PINS;
static uint8_t active_pin_count = BUTTON_COUNT;

enum class State { IDLE, RECORDING, UPLOADING };
static State state = State::IDLE;
static unsigned long record_start_ms = 0;
static const unsigned long MIN_RECORD_MS = 500;
static unsigned long MAX_RECORD_MS = 60000; // updated from config

static uint8_t active_button_index = 0; // which button triggered recording
static unsigned long upload_start_ms = 0;

// ── LED helpers ─────────────────────────────────────

static void led_set(uint8_t r, uint8_t g, uint8_t b) {
    led.setPixelColor(0, led.Color(r, g, b));
    led.show();
}

static void led_blink(uint8_t r, uint8_t g, uint8_t b, unsigned long period_ms) {
    bool on = (millis() / (period_ms / 2)) % 2 == 0;
    led_set(on ? r : 0, on ? g : 0, on ? b : 0);
}

// ── SETUP ───────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== ESP32-S3 Intercom Button (Multi) ===");

    led.begin();
    led_set(0, 0, 0);
    led.setBrightness(32);

    // ── Load runtime config from LittleFS ───────────
    ConfigManager::begin();
    MAX_RECORD_MS = ConfigManager::max_record_secs() * 1000UL;

    Serial.printf("Server: %s:%u | Max: %us\n", ConfigManager::server_host(), ConfigManager::server_port(),
                  ConfigManager::max_record_secs());

    // ── Per-button room mapping (NVS + config.json) ─
    if (!room_store.begin()) {
        Serial.println("[main] NVS init failed — using defaults");
    }
    ConfigManager::load_button_defaults(room_store);

    if (ConfigManager::active_pin_count() > 0) {
        active_pins = ConfigManager::active_pins();
        active_pin_count = ConfigManager::active_pin_count();
    }

    for (uint8_t i = 0; i < active_pin_count; i++) {
        Serial.printf("[main] GPIO%u → %s\n", active_pins[i], room_store.get_room(active_pins[i]).c_str());
    }

    // ── Button manager ──────────────────────────────
    buttons.begin(active_pins, active_pin_count);

    // ── WiFi ────────────────────────────────────────
    WiFiManager::begin(ConfigManager::wifi_ssid(), ConfigManager::wifi_password());

    // ── Audio recorder ──────────────────────────────
    if (!recorder.begin(ConfigManager::sample_rate(), ConfigManager::max_record_secs())) {
        Serial.println("FATAL: AudioRecorder init failed");
        while (1) {
            led_blink(255, 0, 0, 200);
            delay(10);
        }
    }

    Serial.println("Setup complete — ready.");
}

// ── LOOP ────────────────────────────────────────────

void loop() {
    bool wifi_ok = WiFiManager::update();
    auto event = buttons.poll();

    switch (state) {

        case State::IDLE: {
            if (!wifi_ok) {
                led_blink(255, 0, 0, 500);
            } else {
                led_set(0, 255, 0);
            }

            // PTT triggers on PRESS (immediate) for responsive UX.
            // Single-threaded loop(): no race on active_button_index —
            // ButtonManager::poll() runs synchronously, ISR only touches
            // volatile flags inside ButtonManager.
            if (event.type == ButtonManager::EventType::PRESS && wifi_ok) {
                active_button_index = event.button_index;
                recorder.start();
                record_start_ms = millis();
                led_set(0, 0, 255);
                state = State::RECORDING;
                Serial.printf("[main] Recording for GPIO%u...\n", active_pins[active_button_index]);
            }
            break;
        }

        case State::RECORDING: {
            led_set(0, 0, 255);
            unsigned long elapsed = millis() - record_start_ms;

            if (elapsed >= MAX_RECORD_MS) {
                Serial.println("[main] Max time reached — stopping");
                goto stop_and_upload;
            }

            if (event.type == ButtonManager::EventType::RELEASE && event.button_index == active_button_index) {
                Serial.printf("[main] Released after %lu ms\n", elapsed);
            stop_and_upload:
                recorder.stop();

                if (elapsed < MIN_RECORD_MS) {
                    Serial.println("[main] Too short — discarding");
                    led_set(0, 255, 0);
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
            led_blink(255, 255, 255, 100);

            uint8_t gpio = active_pins[active_button_index];
            std::string room = room_store.get_room(gpio);

            bool ok = HTTPUploader::upload(recorder.data(), recorder.total_bytes(), ConfigManager::server_host(),
                                           ConfigManager::server_port(), room.c_str());

            unsigned long upload_ms = millis() - upload_start_ms;
            Serial.printf("[main] Upload to %s %s (%lu ms)\n", room.c_str(), ok ? "OK" : "FAILED", upload_ms);

            for (int i = 0; i < 4; i++) {
                led_set(ok ? 0 : 255, ok ? 255 : 0, 0);
                delay(125);
                led_set(0, 0, 0);
                delay(125);
            }

            state = State::IDLE;
            break;
        }

    } // switch

    delay(10);
}
