#pragma once
#include <cstdint>

/// Manages N physical buttons with per-button debounce and event detection.
///
/// Each button is active LOW with internal pull-up.
/// Call poll() in loop() to get press/release/long-press events.
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

    /// Initialize with GPIO pin array and button count.
    /// Configures each pin as INPUT_PULLUP.
    void begin(const uint8_t* pins, uint8_t count);

    /// Poll for the next button event. Returns Event with type=NONE if no event.
    /// Processes all buttons each call — returns only the first event found.
    Event poll();

    /// True if button is currently held down (debounced active state).
    bool is_held(uint8_t index) const;

    /// Duration the button has been held, in ms. 0 if not held.
    unsigned long hold_duration(uint8_t index) const;

    /// Number of buttons managed.
    uint8_t count() const {
        return _count;
    }

private:
    struct Button {
        uint8_t gpio;
        bool raw = true;    // last raw GPIO reading (true=HIGH)
        bool stable = true; // debounced stable state
        bool dirty = false; // raw differs from stable, debouncing
        unsigned long dirty_since = 0;
        bool held = false; // currently in active press
        unsigned long press_start = 0;
        bool long_fired = false; // LONG_PRESS already emitted for this press
    };

    Button* _buttons = nullptr;
    uint8_t _count = 0;
};
