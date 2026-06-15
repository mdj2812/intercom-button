#pragma once

// ──── WiFi ──────────────────────────────────────────
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// ──── Flask intercom server ─────────────────────────
#define SERVER_HOST   "192.168.99.x"
#define SERVER_PORT   8764
#define API_PATH      "/convert"

// ──── Room target — change per device ───────────────
#define ROOM_TARGET   "书房"

// ──── Audio ─────────────────────────────────────────
#define SAMPLE_RATE     16000      // Hz
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS    1
#define MAX_RECORD_SECS 10         // safety timeout

// ──── Pin mapping ───────────────────────────────────
#define PIN_BUTTON      4          // GPIO4 → button (active low, internal pull-up)
#define PIN_LED_WS2812  48         // GPIO48 → built-in RGB LED (WS2812)
#define PIN_ADC_CHANNEL ADC_CHANNEL_0  // GPIO1 = ADC1_CH0

// ──── MAX9814 gain (documentation; set via module pads) ──
// GAIN = GND  → 50dB (recommended for desktop)
// GAIN = VCC  → 40dB
// GAIN = open → 60dB (default on most modules)
