/// Unit tests: real WiFiManager through mocks.
///
/// Tests the actual src/wifi_manager.cpp code using the
/// mock WiFi and Arduino headers from test/mocks/.

#include <Arduino.h>
#include "wifi_manager.h"
#include <unity.h>

void setUp() {
    mock_wifi_reset();
    mock_set_millis(0);
}

void tearDown() {
}

// ── Test 1: begin() sets WiFi parameters ─────────────
void test_begin_sets_wifi_params() {
    WiFiManager::begin("MySSID", "secret");

    // Verify WiFi was configured correctly through the mock globals
    TEST_ASSERT_EQUAL(WIFI_STA, _wifi_mock.mode);
    TEST_ASSERT_TRUE(_wifi_mock.auto_reconnect);
    TEST_ASSERT_EQUAL_STRING("MySSID", _wifi_mock.ssid);
}

// ── Test 2: update() returns true when connected ─────
void test_update_returns_true_when_connected() {
    mock_wifi_set_connected(true, -55);
    TEST_ASSERT_TRUE(WiFiManager::update());
}

// ── Test 3: update() retries only after interval ─────
void test_update_retries_after_reconnect_interval() {
    mock_wifi_set_connected(false); // disconnected
    mock_set_millis(0);

    // First call: triggers reconnect (next_reconnect_ms starts at 0)
    TEST_ASSERT_FALSE(WiFiManager::update());
    // Second call immediately: interval not elapsed, no reconnect
    TEST_ASSERT_FALSE(WiFiManager::update());

    // Advance past RECONNECT_INTERVAL (5000 ms)
    mock_advance_millis(5001);

    // Now interval elapsed — should trigger reconnect again
    TEST_ASSERT_FALSE(WiFiManager::update());
    // And immediately again should not
    TEST_ASSERT_FALSE(WiFiManager::update());
}

// ── Test 4: rssi() returns mock rssi when connected ──
void test_rssi_returns_mock_value_when_connected() {
    mock_wifi_set_connected(true, -72);
    TEST_ASSERT_EQUAL(-72, WiFiManager::rssi());
}

// ── Test 5: rssi() returns 0 when disconnected ───────
void test_rssi_returns_zero_when_disconnected() {
    mock_wifi_set_connected(false);
    TEST_ASSERT_EQUAL(0, WiFiManager::rssi());
}

// ── Test 6: status_str() returns correct strings ─────
void test_status_str_returns_correct_strings() {
    mock_wifi_set_connected(true);
    TEST_ASSERT_EQUAL_STRING("connected", WiFiManager::status_str());

    mock_wifi_reset(); // back to WL_DISCONNECTED (default)
    TEST_ASSERT_EQUAL_STRING("disconnected", WiFiManager::status_str());

    // Test other status codes directly via the mock state
    _wifi_mock.status = WL_NO_SSID_AVAIL;
    TEST_ASSERT_EQUAL_STRING("no_ssid", WiFiManager::status_str());

    _wifi_mock.status = WL_CONNECT_FAILED;
    TEST_ASSERT_EQUAL_STRING("connect_failed", WiFiManager::status_str());

    _wifi_mock.status = WL_IDLE_STATUS;
    TEST_ASSERT_EQUAL_STRING("idle", WiFiManager::status_str());

    _wifi_mock.status = 99; // unknown code
    TEST_ASSERT_EQUAL_STRING("unknown", WiFiManager::status_str());
}

// ── Bonus: is_connected() inline function ─────────────
void test_is_connected_matches_wifi_status() {
    mock_wifi_set_connected(true);
    TEST_ASSERT_TRUE(WiFiManager::is_connected());

    mock_wifi_set_connected(false);
    TEST_ASSERT_FALSE(WiFiManager::is_connected());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_sets_wifi_params);
    RUN_TEST(test_update_returns_true_when_connected);
    RUN_TEST(test_update_retries_after_reconnect_interval);
    RUN_TEST(test_rssi_returns_mock_value_when_connected);
    RUN_TEST(test_rssi_returns_zero_when_disconnected);
    RUN_TEST(test_status_str_returns_correct_strings);
    RUN_TEST(test_is_connected_matches_wifi_status);
    return UNITY_END();
}
