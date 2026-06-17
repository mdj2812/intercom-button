#pragma once
#include <cstdint>
#include <memory>

// IRAM_ATTR is ESP32-specific; no-op on native
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

/// Manages N physical buttons with per-button GPIO interrupt + debounce.
///
/// Each button is active LOW with internal pull-up. GPIO CHANGE interrupts
/// wake the CPU only when a button actually toggles — zero polling overhead
/// when idle. Call poll() in loop() to consume queued events.
class ButtonManager {
public:
    static constexpr unsigned long DEBOUNCE_MS = 50;
    static constexpr unsigned long LONG_PRESS_MS = 2000;

    enum class EventType { NONE, PRESS, RELEASE, LONG_PRESS };

    struct Event {
        uint8_t button_index;
        EventType type;

        bool valid() const {
            return type != EventType::NONE;
        }
    };

    ~ButtonManager();

    // ── No copy / move ────────────────────────────
    ButtonManager() = default;
    ButtonManager(const ButtonManager&) = delete;
    ButtonManager& operator=(const ButtonManager&) = delete;

    /// Initialize with GPIO pin array and button count.
    /// Configures each pin as INPUT_PULLUP and attaches CHANGE interrupt.
    void begin(const uint8_t* pins, uint8_t count);

    /// Consume the next button event from the ISR queue.
    /// Returns Event with type=NONE if no pending event.
    /// O(1) when idle — no digitalRead() calls if no interrupt fired.
    Event poll();

    /// True if button is currently held down (debounced active state).
    bool is_held(uint8_t index) const;

    /// Duration the button has been held, in ms. 0 if not held.
    unsigned long hold_duration(uint8_t index) const;

    /// Number of buttons managed.
    uint8_t count() const {
        return _count;
    }

#ifdef UNIT_TEST
    /// For testing: simulate a GPIO edge as if the ISR fired.
    /// Sets the recorded level and marks the index as pending.
    void _simulate_change(uint8_t index, bool level);
#endif

private:
    struct Button {
        uint8_t gpio;

        // ── ISR-updated (volatile) ─────────────
        volatile bool irq_pending = false;   // set by ISR, cleared by poll()
        volatile unsigned long irq_time = 0; // millis() captured in ISR
        volatile bool irq_level = true;      // digitalRead() captured in ISR (true=HIGH)

        // ── State machine (poll() only) ───────
        bool stable = true; // debounced state (true=HIGH / not pressed)
        bool held = false;
        unsigned long press_start = 0;
        bool long_fired = false;
    };

    static void IRAM_ATTR _isr(void* arg);

    std::unique_ptr<Button[]> _buttons;
    uint8_t _count = 0;
};
