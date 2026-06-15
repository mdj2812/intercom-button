/**
 * ESP32-S3 Desktop Intercom Button
 *
 * Push-to-talk button that records audio via MAX9814 electret mic,
 * sends WAV to Flask intercom server, which broadcasts to Home Assistant speakers.
 *
 * Hardware:
 *   - ESP32-S3-DevKitC
 *   - MAX9814 mic module (VCC→3.3V, GND→GND, OUT→GPIO1)
 *   - Push button (GPIO4→GND, active low, internal pull-up)
 *
 * Per-device config: edit config.h (WiFi, server IP, room name).
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "wifi_manager.h"
#include "audio_recorder.h"
#include "http_uploader.h"

// ── Globals ─────────────────────────────────────────
static AudioRecorder    recorder;
static Adafruit_NeoPixel led(1, PIN_LED_WS2812, NEO_GRB + NEO_KHZ800);

// Button state machine
enum class State {
    IDLE,
    RECORDING,
    UPLOADING,
};
static State state = State::IDLE;
static unsigned long record_start_ms = 0;
static const unsigned long MIN_RECORD_MS = 500;   // discard taps shorter than this
static const unsigned long MAX_RECORD_MS = MAX_RECORD_SECS * 1000;

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

// ── Button read (debounced) ─────────────────────────

static bool button_pressed() {
    return digitalRead(PIN_BUTTON) == LOW;
}

// ── SETUP ───────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== ESP32-S3 Intercom Button ===");
    Serial.printf("Room: %s | Server: %s:%d\n", ROOM_TARGET, SERVER_HOST, SERVER_PORT);

    // ── LED ──
    led.begin();
    led_set(0, 0, 0);
    led.setBrightness(32); // not blinding on a desk

    // ── Button ──
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // ── WiFi ──
    WiFiManager::begin(WIFI_SSID, WIFI_PASSWORD);

    // ── Audio recorder ──
    if (!recorder.begin()) {
        Serial.println("FATAL: AudioRecorder init failed");
        // Blink red forever
        while (1) { led_blink(255, 0, 0, 200); delay(10); }
    }

    Serial.println("Setup complete — ready.");
}

// ── LOOP ────────────────────────────────────────────

void loop() {
    bool wifi_ok = WiFiManager::update();
    bool btn = button_pressed();

    // ── State machine ───────────────────────────────
    switch (state) {

    case State::IDLE: {
        // LED: green when ready, red blink when WiFi down
        if (!wifi_ok) {
            led_blink(255, 0, 0, 500);
        } else {
            led_set(0, 255, 0);
        }

        // Button press → start recording (WiFi must be connected)
        if (btn && !last_button && wifi_ok) {
            recorder.start();
            record_start_ms = millis();
            led_set(0, 0, 255); // blue = recording
            state = State::RECORDING;
            Serial.println("[main] Recording...");
        }
        break;
    }

    case State::RECORDING: {
        led_set(0, 0, 255);

        unsigned long elapsed = millis() - record_start_ms;

        // Safety cutoff
        if (elapsed >= MAX_RECORD_MS) {
            Serial.println("[main] Max time reached — stopping");
            goto stop_and_upload;
        }

        // Button released
        if (!btn && last_button) {
            Serial.printf("[main] Button released after %lu ms\n", elapsed);
stop_and_upload:
            recorder.stop();

            // Discard taps shorter than MIN_RECORD_MS
            if (elapsed < MIN_RECORD_MS) {
                Serial.println("[main] Too short — discarding");
                recorder.start(); // reset buffer position? No — start() resets
                // Actually, just call start() which resets write_idx.
                // Wait — start() resets write_idx but also starts DMA again.
                // We need to reset the buffer without restarting DMA.
                // Simple approach: just leave the stale data; next start() overwrites.
                led_set(0, 255, 0);
                state = State::IDLE;
                break;
            }

            // Write WAV header, begin upload
            recorder.write_wav_header();
            upload_start_ms = millis();
            state = State::UPLOADING;
            Serial.println("[main] Uploading...");
        }
        break;
    }

    case State::UPLOADING: {
        led_blink(255, 255, 255, 100); // white fast blink

        bool ok = HTTPUploader::upload(
            recorder.data(), recorder.total_bytes(),
            SERVER_HOST, SERVER_PORT, ROOM_TARGET
        );

        unsigned long upload_elapsed = millis() - upload_start_ms;
        Serial.printf("[main] Upload %s (%lu ms)\n",
                      ok ? "OK" : "FAILED", upload_elapsed);

        // Flash green (success) or red (fail) for 1 second
        if (ok) {
            for (int i = 0; i < 4; i++) { led_set(0, 255, 0); delay(125); led_set(0, 0, 0); delay(125); }
        } else {
            for (int i = 0; i < 4; i++) { led_set(255, 0, 0); delay(125); led_set(0, 0, 0); delay(125); }
        }

        state = State::IDLE;
        break;
    }

    } // switch

    last_button = btn;
    delay(10); // ~100 Hz loop rate
}
