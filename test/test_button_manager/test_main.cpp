/// ButtonManager native tests — validates debounce, press/release/long-press detection.
///
/// Uses mock digitalRead/millis to simulate button presses without real hardware.

#include "mocks/Arduino.h"
#include <unity.h>
#include <cstring>

// ── Include after mocks ─────────────────────────────
#include "../src/button_manager.h"

using EventType = ButtonManager::EventType;

static ButtonManager mgr;
static const uint8_t PINS[] = {4, 5, 12, 13};

// Shorthand for setting mock pin state
static void set_pin(uint8_t pin, uint8_t val) { digitalReadPin[pin] = val; }
static void advance_ms(unsigned long ms) { _mock_millis += ms; }

void setUp() {
    // Reset mocks
    for (auto& v : digitalReadPin) v = HIGH;
    for (auto& v : pinModePinParam) v = 0;
    _mock_millis = 0;

    mgr = ButtonManager();
    mgr.begin(PINS, 4);
}

// ── Test: initialization ────────────────────────────

void test_begin_initializes_count() {
    TEST_ASSERT_EQUAL(4, mgr.count());
}

void test_begin_configures_input_pullup() {
    TEST_ASSERT_EQUAL(INPUT_PULLUP, pinModePinParam[4]);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, pinModePinParam[5]);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, pinModePinParam[12]);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, pinModePinParam[13]);
}

// ── Test: debounce ──────────────────────────────────

void test_no_event_when_idle() {
    set_pin(4, HIGH);
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid());
}

void test_press_detected_after_debounce() {
    // Start: all HIGH
    set_pin(4, HIGH);
    mgr.poll();

    // Button pressed (goes LOW)
    set_pin(4, LOW);
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid()); // still debouncing

    // Advance past debounce period while held LOW
    advance_ms(60);
    ev = mgr.poll();
    TEST_ASSERT_TRUE(ev.valid());
    TEST_ASSERT_EQUAL(EventType::PRESS, ev.type);
    TEST_ASSERT_EQUAL(0, ev.button_index);
}

void test_bounce_filtered() {
    set_pin(4, HIGH);
    mgr.poll();

    // Contact bounce: LOW → HIGH → LOW within debounce window
    set_pin(4, LOW);
    mgr.poll();

    advance_ms(10);
    set_pin(4, HIGH); // bounce back
    mgr.poll();

    advance_ms(10);
    set_pin(4, LOW); // bounce again
    mgr.poll();

    // Still no event — each bounce resets timer
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid());

    // Now hold steady LOW past debounce
    advance_ms(60);
    ev = mgr.poll();
    TEST_ASSERT_TRUE(ev.valid());
    TEST_ASSERT_EQUAL(EventType::PRESS, ev.type);
}

// ── Test: release ───────────────────────────────────

void test_release_detected_after_debounce() {
    // Press button first
    set_pin(4, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS event

    // Release button (goes HIGH)
    set_pin(4, HIGH);
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid()); // debouncing

    advance_ms(60);
    ev = mgr.poll();
    TEST_ASSERT_TRUE(ev.valid());
    TEST_ASSERT_EQUAL(EventType::RELEASE, ev.type);
    TEST_ASSERT_EQUAL(0, ev.button_index);
}

// ── Test: long press ────────────────────────────────

void test_long_press_emitted_after_threshold() {
    set_pin(4, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS

    // Advance to just before long-press threshold
    advance_ms(1900);
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid()); // not yet

    // Cross threshold
    advance_ms(200);
    ev = mgr.poll();
    TEST_ASSERT_TRUE(ev.valid());
    TEST_ASSERT_EQUAL(EventType::LONG_PRESS, ev.type);
}

void test_long_press_fires_only_once() {
    set_pin(4, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS

    // First long press
    advance_ms(2100);
    auto ev = mgr.poll();
    TEST_ASSERT_EQUAL(EventType::LONG_PRESS, ev.type);

    // Still held, but no more long press events
    advance_ms(1000);
    ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid());
}

// ── Test: is_held & hold_duration ───────────────────

void test_is_held_and_duration() {
    set_pin(4, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS

    TEST_ASSERT_TRUE(mgr.is_held(0));
    TEST_ASSERT_FALSE(mgr.is_held(1));

    advance_ms(500);
    TEST_ASSERT_EQUAL(500, mgr.hold_duration(0)); // press_start at 60ms, now at 560ms

    // Release
    set_pin(4, HIGH);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // RELEASE

    TEST_ASSERT_FALSE(mgr.is_held(0));
    TEST_ASSERT_EQUAL(0, mgr.hold_duration(0));
}

// ── Test: multiple buttons ──────────────────────────

void test_independent_buttons() {
    // Button 0 pressed
    set_pin(4, LOW);
    mgr.poll();
    advance_ms(60);
    auto ev = mgr.poll();
    TEST_ASSERT_EQUAL(EventType::PRESS, ev.type);
    TEST_ASSERT_EQUAL(0, ev.button_index);

    // Button 2 pressed while button 0 still held
    set_pin(12, LOW);
    advance_ms(10);
    mgr.poll();
    advance_ms(60);
    ev = mgr.poll();
    TEST_ASSERT_EQUAL(EventType::PRESS, ev.type);
    TEST_ASSERT_EQUAL(2, ev.button_index);

    TEST_ASSERT_TRUE(mgr.is_held(0));
    TEST_ASSERT_TRUE(mgr.is_held(2));
    TEST_ASSERT_FALSE(mgr.is_held(1));
}

// ── Runner ───────────────────────────────────────────

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_begin_initializes_count);
    RUN_TEST(test_begin_configures_input_pullup);
    RUN_TEST(test_no_event_when_idle);
    RUN_TEST(test_press_detected_after_debounce);
    RUN_TEST(test_bounce_filtered);
    RUN_TEST(test_release_detected_after_debounce);
    RUN_TEST(test_long_press_emitted_after_threshold);
    RUN_TEST(test_long_press_fires_only_once);
    RUN_TEST(test_is_held_and_duration);
    RUN_TEST(test_independent_buttons);

    return UNITY_END();
}
