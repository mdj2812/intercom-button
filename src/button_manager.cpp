#include "button_manager.h"
#include <Arduino.h>

void ButtonManager::begin(const uint8_t* pins, uint8_t count) {
    _count = count;
    _buttons = new Button[count];

    for (uint8_t i = 0; i < count; i++) {
        _buttons[i].gpio = pins[i];
        pinMode(pins[i], INPUT_PULLUP);
    }
}

ButtonManager::Event ButtonManager::poll() {
    const unsigned long now = millis();

    for (uint8_t i = 0; i < _count; i++) {
        auto& b = _buttons[i];
        bool raw = digitalRead(b.gpio) == HIGH; // true = not pressed (pull-up)

        // ── Debounce ─────────────────────────
        if (raw != b.raw) {
            b.raw = raw;
            b.dirty = true;
            b.dirty_since = now;
            continue; // wait for debounce period
        }

        if (b.dirty && (now - b.dirty_since >= DEBOUNCE_MS)) {
            b.dirty = false;

            // ── State transition ─────────────
            if (raw != b.stable) {
                b.stable = raw;

                if (!raw) {
                    // HIGH → LOW → PRESS
                    b.held = true;
                    b.press_start = now;
                    b.long_fired = false;
                    return {i, EventType::PRESS};
                } else {
                    // LOW → HIGH → RELEASE
                    b.held = false;
                    b.long_fired = false;
                    return {i, EventType::RELEASE};
                }
            }
        }

        // ── Long press detection ────────────
        if (b.held && !b.long_fired && (now - b.press_start >= LONG_PRESS_MS)) {
            b.long_fired = true;
            return {i, EventType::LONG_PRESS};
        }
    }

    return {0, EventType::NONE};
}

bool ButtonManager::is_held(uint8_t index) const {
    if (index >= _count) return false;
    return _buttons[index].held;
}

unsigned long ButtonManager::hold_duration(uint8_t index) const {
    if (index >= _count || !_buttons[index].held) return 0;
    return millis() - _buttons[index].press_start;
}
