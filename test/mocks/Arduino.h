#pragma once
#include "Print.h"
#include "Stream.h"
#include "WString.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#ifndef SET_LOOP_TASK_STACK_SIZE
#define SET_LOOP_TASK_STACK_SIZE(bytes) static_assert(true, "native")
#endif

// ── Pin I/O ─────────────────────────────────────────
#define INPUT 0x01
#define OUTPUT 0x02
#define INPUT_PULLUP 0x05
#define INPUT_PULLDOWN 0x06

#define LOW 0
#define HIGH 1
#define CHANGE 1 // interrupt mode

// ── Test-controllable pin state ────────────────────
// Tests set digitalReadPin[pin] to control digitalRead() return value.
// Tests read pinModePinParam[pin] to verify pinMode() was called.
constexpr uint8_t MAX_MOCK_PIN = 64;

inline uint8_t digitalReadPin[MAX_MOCK_PIN] = {HIGH}; // default: not pressed
inline uint8_t pinModePinParam[MAX_MOCK_PIN] = {0};

inline void pinMode(uint8_t pin, uint8_t mode) {
    pinModePinParam[pin] = mode;
}
inline int digitalRead(uint8_t pin) {
    return digitalReadPin[pin];
}
inline void digitalWrite(uint8_t pin, uint8_t val) {
    (void) pin;
    (void) val;
}

// ── Interrupt stubs (no-op in native tests) ────────────
// The real ISR path is exercised via ButtonManager::_simulate_change().
inline void attachInterruptArg(uint8_t pin, void (*)(void*), void*, int) {
    (void) pin;
}
inline void detachInterrupt(uint8_t pin) {
    (void) pin;
}

// ── Timing — mockable via inline globals ────────────
inline unsigned long _mock_millis = 0;

inline unsigned long millis() {
    return _mock_millis;
}
inline unsigned long micros() {
    return _mock_millis * 1000;
}
inline void delay(unsigned long ms) {
    _mock_millis += ms;
}

inline void mock_set_millis(unsigned long ms) {
    _mock_millis = ms;
}
inline void mock_advance_millis(unsigned long ms) {
    _mock_millis += ms;
}

// ── Serial mock ─────────────────────────────────────
class SerialMock : public Stream {
    std::string _rx;

public:
    void begin(unsigned long baud) {
        (void) baud;
    }
    using Print::printf;

    int available() override {
        return static_cast<int>(_rx.size());
    }

    void mock_push(const char* s) {
        if (s)
            _rx += s;
    }

    void mock_clear_rx() {
        _rx.clear();
    }

    String readStringUntil(char terminator) {
        auto pos = _rx.find(terminator);
        String out;
        if (pos == std::string::npos) {
            out = String(_rx);
            _rx.clear();
        } else {
            out = String(_rx.substr(0, pos));
            _rx.erase(0, pos + 1);
        }
        return out;
    }
};
inline SerialMock Serial;

// ── Hardware timer (Arduino-ESP32 v2 API) ───────────
struct hw_timer_t {
    void (*isr)() = nullptr;
    bool alarm_enabled = false;
};

inline hw_timer_t _mock_hw_timer;
inline bool _mock_timer_begin_fail = false;

inline void mock_timer_reset() {
    _mock_hw_timer = hw_timer_t{};
    _mock_timer_begin_fail = false;
}

inline hw_timer_t* timerBegin(uint8_t, uint16_t, bool) {
    if (_mock_timer_begin_fail)
        return nullptr;
    _mock_hw_timer = hw_timer_t{};
    return &_mock_hw_timer;
}

inline void timerEnd(hw_timer_t* timer) {
    if (timer)
        *timer = hw_timer_t{};
}

inline void timerAttachInterrupt(hw_timer_t* timer, void (*fn)(), bool) {
    if (timer)
        timer->isr = fn;
}

inline void timerAlarmWrite(hw_timer_t*, uint64_t, bool) {}

inline void timerAlarmEnable(hw_timer_t* timer) {
    if (timer)
        timer->alarm_enabled = true;
}

inline void timerAlarmDisable(hw_timer_t* timer) {
    if (timer)
        timer->alarm_enabled = false;
}

inline void mock_timer_isr() {
    if (_mock_hw_timer.isr && _mock_hw_timer.alarm_enabled)
        _mock_hw_timer.isr();
}

// ── ESP.restart ─────────────────────────────────────
inline int _mock_esp_restart_count = 0;

struct EspClass {
    void restart() {
        _mock_esp_restart_count++;
    }
};
inline EspClass ESP;

inline void mock_esp_reset() {
    _mock_esp_restart_count = 0;
}

// ── printf implementation ───────────────────────────
inline int Print::printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}
