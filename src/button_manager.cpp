#include "button_manager.h"
#include <Arduino.h>

// ── Static ISR trampoline ─────────────────────────────

void IRAM_ATTR ButtonManager::_isr(void* arg) {
    auto* b = static_cast<Button*>(arg);
    b->irq_level = digitalRead(b->gpio);
    b->irq_time  = millis();
    b->irq_pending = true;
}

// ── Lifecycle ─────────────────────────────────────────

ButtonManager::~ButtonManager() {
    for (uint8_t i = 0; i < _count; i++) {
        detachInterrupt(_buttons[i].gpio);
    }
    delete[] _buttons;
}

void ButtonManager::begin(const uint8_t* pins, uint8_t count) {
    _count   = count;
    _buttons = new Button[count];

    for (uint8_t i = 0; i < count; i++) {
        _buttons[i].gpio = pins[i];
        pinMode(pins[i], INPUT_PULLUP);
        attachInterruptArg(pins[i], _isr, &_buttons[i], CHANGE);
    }
}

// ── Event consumption ─────────────────────────────────

ButtonManager::Event ButtonManager::poll() {
    const unsigned long now = millis();

    // Phase 1 — process interrupt-driven level changes
    for (uint8_t i = 0; i < _count; i++) {
        auto& b = _buttons[i];
        if (!b.irq_pending)
            continue;

        // Snapshot volatile data (flag NOT cleared yet)
        bool         level  = b.irq_level;
        unsigned long irq_t = b.irq_time;

        if (now - irq_t < DEBOUNCE_MS)
            continue;            // still within debounce window

        // Debounce passed — clear flag now, then process
        b.irq_pending = false;   // allow ISR to re-trigger for this button

        if (level != b.stable) {
            b.stable = level;

            if (!level) {
                // HIGH → LOW  =  PRESS
                b.held        = true;
                b.press_start = now;
                b.long_fired  = false;
                return {i, EventType::PRESS};
            } else {
                // LOW → HIGH  =  RELEASE
                b.held       = false;
                b.long_fired = false;
                return {i, EventType::RELEASE};
            }
        }
    }

    // Phase 2 — long press (timer-based, no edge needed)
    for (uint8_t i = 0; i < _count; i++) {
        auto& b = _buttons[i];
        if (b.held && !b.long_fired && (now - b.press_start >= LONG_PRESS_MS)) {
            b.long_fired = true;
            return {i, EventType::LONG_PRESS};
        }
    }

    return {0, EventType::NONE};
}

// ── Query ─────────────────────────────────────────────

bool ButtonManager::is_held(uint8_t index) const {
    if (index >= _count)
        return false;
    return _buttons[index].held;
}

unsigned long ButtonManager::hold_duration(uint8_t index) const {
    if (index >= _count || !_buttons[index].held)
        return 0;
    return millis() - _buttons[index].press_start;
}

// ── Testing helper ────────────────────────────────────

void ButtonManager::_simulate_change(uint8_t index, bool level) {
    if (index >= _count)
        return;
    _buttons[index].irq_level  = level;
    _buttons[index].irq_time   = millis();
    _buttons[index].irq_pending = true;
}
