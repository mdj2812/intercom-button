#pragma once
#include <cstdint>

#define NEO_GRB 0
#define NEO_KHZ800 0

using neoPixelType = uint16_t;

class Adafruit_NeoPixel {
public:
    uint32_t last_color = 0;
    uint8_t brightness = 255;
    bool began = false;

    Adafruit_NeoPixel(uint16_t, int16_t, neoPixelType) {}

    void begin() {
        began = true;
    }
    void show() {}
    void setBrightness(uint8_t b) {
        brightness = b;
    }
    void setPixelColor(uint16_t, uint32_t c) {
        last_color = c;
    }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
    }
};
