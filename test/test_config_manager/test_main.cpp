/// Integration test: real ConfigManager through mocks.
///
/// ConfigManager uses static state — begin() only overwrites keys present
/// in the JSON file. Missing file or parse errors leave previous values unchanged.
/// This tests the ACTUAL behavior, not an idealized version.

#include "config_manager.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

static void inject_and_load(const char* json) {
    LittleFS.reset();
    LittleFS.begin(true);
    LittleFS.inject("/config.json", json);
    ConfigManager::begin();
}

// ── Test 1: Full config, all keys present ───────────
void test_full_config() {
    inject_and_load(R"({
        "wifi_ssid": "MyWiFi",
        "wifi_password": "pass",
        "server_scheme": "https",
        "server_host": "10.0.0.1",
        "server_port": 9999,
        "room": "study",
        "sample_rate": 8000,
        "max_record_secs": 30
    })");

    TEST_ASSERT_EQUAL_STRING("MyWiFi", ConfigManager::wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("study", ConfigManager::room_target());
    TEST_ASSERT_EQUAL_STRING("https", ConfigManager::server_scheme());
    TEST_ASSERT_EQUAL_STRING("10.0.0.1", ConfigManager::server_host());
    TEST_ASSERT_EQUAL(9999, ConfigManager::server_port());
    TEST_ASSERT_EQUAL(8000, ConfigManager::sample_rate());
    TEST_ASSERT_EQUAL(30, ConfigManager::max_record_secs());
}

// ── Test 2: Scheme is normalized and invalid values are ignored ──
void test_server_scheme_validation() {
    inject_and_load(R"({"server_scheme": "HTTPS"})");
    TEST_ASSERT_EQUAL_STRING("https", ConfigManager::server_scheme());

    inject_and_load(R"({"server_scheme": "ftp"})");
    TEST_ASSERT_EQUAL_STRING("https", ConfigManager::server_scheme());
}

// ── Test 3: Partial config — only keys in JSON change ──
void test_partial_config_only_overwrites_present_keys() {
    // First inject full config
    inject_and_load(R"({
        "wifi_ssid": "FullWiFi",
        "server_host": "192.168.1.1",
        "room": "living"
    })");

    // Now inject partial config — only room changes
    inject_and_load(R"({"room": "bedroom"})");

    // room changed, wifi_ssid and server_host retained from previous
    TEST_ASSERT_EQUAL_STRING("bedroom", ConfigManager::room_target());
    TEST_ASSERT_EQUAL_STRING("FullWiFi", ConfigManager::wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", ConfigManager::server_host());
}

// ── Test 4: Invalid JSON — previous values keep ─────
void test_invalid_json_keeps_previous_values() {
    // Inject valid config
    inject_and_load(R"({"room": "cinema", "wifi_ssid": "StableWiFi"})");

    // Then broken JSON — begin() returns false, cfg unchanged
    LittleFS.reset();
    LittleFS.begin(true);
    LittleFS.inject("/config.json", "{broken!!");
    ConfigManager::begin();

    TEST_ASSERT_EQUAL_STRING("cinema", ConfigManager::room_target());
    TEST_ASSERT_EQUAL_STRING("StableWiFi", ConfigManager::wifi_ssid());
}

// ── Test 5: Missing file — keeps previous values ─────
void test_missing_file_keeps_prior_state() {
    // Inject first
    inject_and_load(R"({"room": "garage", "sample_rate": 22050})");

    // Then remove file
    LittleFS.reset();
    LittleFS.begin(true);
    ConfigManager::begin(); // file not found

    // Previous state persists (begin() returns early, no reset)
    TEST_ASSERT_EQUAL_STRING("garage", ConfigManager::room_target());
    TEST_ASSERT_EQUAL(22050, ConfigManager::sample_rate());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_config);
    RUN_TEST(test_server_scheme_validation);
    RUN_TEST(test_partial_config_only_overwrites_present_keys);
    RUN_TEST(test_invalid_json_keeps_previous_values);
    RUN_TEST(test_missing_file_keeps_prior_state);
    return UNITY_END();
}
