#pragma once
// ──── Pin mapping (compile-time, same for all devices) ──
#define PIN_BUTTON 4
#define PIN_LED_WS2812 48
#define PIN_ADC_CHANNEL ADC_CHANNEL_0 // GPIO1

// ──── Multi-button pins (compile-time fallback) ──────
constexpr uint8_t BUTTON_PINS[] = {4, 5, 12, 13};
constexpr uint8_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

// ──── MAX9814 gain (set via module solder pads) ────────
// GAIN = GND  → 50dB (recommended)
// GAIN = VCC  → 40dB
// GAIN = open → 60dB (default)
