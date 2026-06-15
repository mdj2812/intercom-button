/**
 * ESP32-S3 Desktop Intercom Button
 *
 * Push-to-talk button that records audio via MAX9814 electret mic,
 * sends WAV to Flask intercom server, which broadcasts to Home Assistant speakers.
 *
 * Configuration is loaded from LittleFS /config.json at boot.
 * Same firmware binary is used for all rooms — just change config.json per device.
 *
 * Hardware:
 *   - ESP32-S3-DevKitC
 *   - MAX9814 mic module (VCC→3.3V, GND→GND, OUT→GPIO1)
 *   - Push button (GPIO4→GND, active low, internal pull-up)
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"            // pin defines only
#include "config_manager.h"
#include "wifi_manager.h"
#include "audio_recorder.h"
#include "http_uploader.h"

// ── Globals ─────────────────────────────────────────
static AudioRecorder    recorder;
static Adafruit_NeoPixel led(1, PIN_LED_WS2812, NEO_GRB + NEO_KHZ800);

enum class State { IDLE, RECORDING, UPLOADING };
static State state = State::IDLE;
static unsigned long record_start_ms = 0;
static const unsigned long MIN_RECORD_MS = 500;
static unsigned long MAX_RECORD_MS = 60000; // updated from config

static bool last_button = HIGH;
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

static bool button_pressed() {
    return digitalRead(PIN_BUTTON) == LOW;
}

// ── SETUP ───────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== ESP32-S3 Intercom Button ===");

    led.begin();
    led_set(0, 0, 0);
    led.setBrightness(32);

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // ── Load runtime config from LittleFS ───────────
    ConfigManager::begin();
    MAX_RECORD_MS = ConfigManager::max_record_secs() * 1000UL;

    Serial.printf("Room: %s | Server: %s:%u | Max: %us\n",
                  ConfigManager::room_target(),
                  ConfigManager::server_host(),
                  ConfigManager::server_port(),
                  ConfigManager::max_record_secs());

    // ── WiFi ────────────────────────────────────────
    WiFiManager::begin(ConfigManager::wifi_ssid(),
                       ConfigManager::wifi_password());

    // ── Audio recorder ──────────────────────────────
    if (!recorder.begin(ConfigManager::sample_rate(),
                        ConfigManager::max_record_secs())) {
        Serial.println("FATAL: AudioRecorder init failed");
        while (1) { led_blink(255, 0, 0, 200); delay(10); }
    }

    Serial.println("Setup complete — ready.");
}

// ── LOOP ────────────────────────────────────────────

void loop() {
    bool wifi_ok = WiFiManager::update();
    bool btn = button_pressed();

    switch (state) {

    case State::IDLE: {
        if (!wifi_ok) {
            led_blink(255, 0, 0, 500);
        } else {
            led_set(0, 255, 0);
        }

        if (btn && !last_button && wifi_ok) {
            recorder.start();
            record_start_ms = millis();
            led_set(0, 0, 255);
            state = State::RECORDING;
            Serial.println("[main] Recording...");
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

        if (!btn && last_button) {
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

        bool ok = HTTPUploader::upload(
            recorder.data(),
            recorder.total_bytes(),
            ConfigManager::server_host(),
            ConfigManager::server_port(),
            ConfigManager::room_target()
        );

        unsigned long upload_ms = millis() - upload_start_ms;
        Serial.printf("[main] Upload %s (%lu ms)\n",
                      ok ? "OK" : "FAILED", upload_ms);

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

    last_button = btn;
    delay(10);
}
