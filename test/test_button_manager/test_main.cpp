/// ButtonManager native tests — validates debounce, press/release/long-press detection.
///
/// Uses ButtonManager::_simulate_change() to mimic GPIO ISR firing,
/// with mock millis() to control time.

#include "mocks/Arduino.h"
#include <cstring>
#include <unity.h>

// ── Include after mocks ─────────────────────────────
#include "../src/button_manager.h"

using EventType = ButtonManager::EventType;

static ButtonManager mgr;
static const uint8_t PINS[] = {4, 5, 12, 13};

// Shorthand: simulate a GPIO edge at the given button index
static void sim_change(uint8_t idx, bool high) {
    mgr._simulate_change(idx, high);
}
static void advance_ms(unsigned long ms) {
    _mock_millis += ms;
}

void setUp() {
    // Reset mocks
    for (auto& v : digitalReadPin)
        v = HIGH;
    for (auto& v : pinModePinParam)
        v = 0;
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

// ── Test: idle ──────────────────────────────────────

void test_no_event_when_idle() {
    // No simulated interrupts — poll() returns NONE immediately
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid());
}

// ── Test: debounce ──────────────────────────────────

void test_press_detected_after_debounce() {
    // Simulate GPIO edge (HIGH → LOW)
    sim_change(0, LOW);
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid()); // within debounce window

    advance_ms(60);
    ev = mgr.poll();
    TEST_ASSERT_TRUE(ev.valid());
    TEST_ASSERT_EQUAL(EventType::PRESS, ev.type);
    TEST_ASSERT_EQUAL(0, ev.button_index);
}

void test_bounce_filtered() {
    // Contact bounce: LOW → HIGH → LOW within debounce window
    sim_change(0, LOW);
    mgr.poll();

    advance_ms(10);
    sim_change(0, HIGH);
    mgr.poll(); // bounce back — resets timer

    advance_ms(10);
    sim_change(0, LOW);
    mgr.poll(); // bounce again

    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid()); // still debouncing

    // Hold steady past debounce
    advance_ms(60);
    ev = mgr.poll();
    TEST_ASSERT_TRUE(ev.valid());
    TEST_ASSERT_EQUAL(EventType::PRESS, ev.type);
}

// ── Test: release ───────────────────────────────────

void test_release_detected_after_debounce() {
    // Press first
    sim_change(0, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS

    // Release (LOW → HIGH)
    sim_change(0, HIGH);
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
    sim_change(0, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS

    // Just before threshold
    advance_ms(1900);
    auto ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid());

    // Cross threshold
    advance_ms(200);
    ev = mgr.poll();
    TEST_ASSERT_TRUE(ev.valid());
    TEST_ASSERT_EQUAL(EventType::LONG_PRESS, ev.type);
}

void test_long_press_fires_only_once() {
    sim_change(0, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS

    advance_ms(2100);
    auto ev = mgr.poll();
    TEST_ASSERT_EQUAL(EventType::LONG_PRESS, ev.type);

    // Still held, no duplicate event
    advance_ms(1000);
    ev = mgr.poll();
    TEST_ASSERT_FALSE(ev.valid());
}

// ── Test: is_held & hold_duration ───────────────────

void test_is_held_and_duration() {
    sim_change(0, LOW);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // PRESS

    TEST_ASSERT_TRUE(mgr.is_held(0));
    TEST_ASSERT_FALSE(mgr.is_held(1));

    advance_ms(500);
    // press_start at 60ms, now at 560ms → duration 500ms
    TEST_ASSERT_EQUAL(500, mgr.hold_duration(0));

    // Release
    sim_change(0, HIGH);
    mgr.poll();
    advance_ms(60);
    mgr.poll(); // RELEASE

    TEST_ASSERT_FALSE(mgr.is_held(0));
    TEST_ASSERT_EQUAL(0, mgr.hold_duration(0));
}

// ── Test: multiple buttons ──────────────────────────

void test_independent_buttons() {
    // Button 0 pressed
    sim_change(0, LOW);
    mgr.poll();
    advance_ms(60);
    auto ev = mgr.poll();
    TEST_ASSERT_EQUAL(EventType::PRESS, ev.type);
    TEST_ASSERT_EQUAL(0, ev.button_index);

    // Button 2 pressed while btn 0 still held
    sim_change(2, LOW);
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
