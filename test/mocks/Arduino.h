#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include "WString.h"
#include "Print.h"
#include "Stream.h"

// ── Pin I/O ─────────────────────────────────────────
#define INPUT           0x01
#define OUTPUT          0x02
#define INPUT_PULLUP    0x05
#define INPUT_PULLDOWN  0x06

#define LOW   0
#define HIGH  1

// ── Test-controllable pin state ────────────────────
// Tests set digitalReadPin[pin] to control digitalRead() return value.
// Tests read pinModePinParam[pin] to verify pinMode() was called.
constexpr uint8_t MAX_MOCK_PIN = 64;

inline uint8_t digitalReadPin[MAX_MOCK_PIN] = {HIGH};  // default: not pressed
inline uint8_t pinModePinParam[MAX_MOCK_PIN] = {0};

inline void pinMode(uint8_t pin, uint8_t mode) {
    pinModePinParam[pin] = mode;
}
inline int digitalRead(uint8_t pin) {
    return digitalReadPin[pin];
}
inline void digitalWrite(uint8_t pin, uint8_t val) {
    (void)pin;
    (void)val;
}

// ── Timing — mockable via inline globals ────────────
inline unsigned long _mock_millis = 0;

inline unsigned long millis() { return _mock_millis; }
inline unsigned long micros() { return _mock_millis * 1000; }
inline void delay(unsigned long ms) { _mock_millis += ms; }

inline void mock_set_millis(unsigned long ms) { _mock_millis = ms; }
inline void mock_advance_millis(unsigned long ms) { _mock_millis += ms; }

// ── Serial mock ─────────────────────────────────────
class SerialMock : public Stream {
public:
    void begin(unsigned long baud) { (void)baud; }
    using Print::printf;
};
inline SerialMock Serial;

// ── printf implementation ───────────────────────────
inline int Print::printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}
