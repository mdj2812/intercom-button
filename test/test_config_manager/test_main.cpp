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
        "sample_rate": 8000,
        "max_record_secs": 30
    })");

    TEST_ASSERT_EQUAL_STRING("MyWiFi", ConfigManager::wifi_ssid());
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
    inject_and_load(R"({
        "wifi_ssid": "FullWiFi",
        "server_host": "192.168.1.1",
        "sample_rate": 8000
    })");

    inject_and_load(R"({"sample_rate": 22050})");

    TEST_ASSERT_EQUAL(22050, ConfigManager::sample_rate());
    TEST_ASSERT_EQUAL_STRING("FullWiFi", ConfigManager::wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", ConfigManager::server_host());
}

// ── Test 4: Invalid JSON — previous values keep ─────
void test_invalid_json_keeps_previous_values() {
    inject_and_load(R"({"wifi_ssid": "StableWiFi", "sample_rate": 8000})");

    LittleFS.reset();
    LittleFS.begin(true);
    LittleFS.inject("/config.json", "{broken!!");
    ConfigManager::begin();

    TEST_ASSERT_EQUAL_STRING("StableWiFi", ConfigManager::wifi_ssid());
    TEST_ASSERT_EQUAL(8000, ConfigManager::sample_rate());
}

// ── Test 5: Missing file — keeps previous values ─────
void test_missing_file_keeps_prior_state() {
    inject_and_load(R"({"wifi_ssid": "GarageWiFi", "sample_rate": 22050})");

    LittleFS.reset();
    LittleFS.begin(true);
    ConfigManager::begin(); // file not found

    TEST_ASSERT_EQUAL_STRING("GarageWiFi", ConfigManager::wifi_ssid());
    TEST_ASSERT_EQUAL(22050, ConfigManager::sample_rate());
}

// ── Test 6: Leftover ha_token in JSON is ignored ──
void test_legacy_ha_token_is_ignored() {
    inject_and_load(R"({
        "wifi_ssid": "TokenWiFi",
        "ha_token": "should-not-be-used"
    })");

    TEST_ASSERT_EQUAL_STRING("TokenWiFi", ConfigManager::wifi_ssid());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_config);
    RUN_TEST(test_server_scheme_validation);
    RUN_TEST(test_partial_config_only_overwrites_present_keys);
    RUN_TEST(test_invalid_json_keeps_previous_values);
    RUN_TEST(test_missing_file_keeps_prior_state);
    RUN_TEST(test_legacy_ha_token_is_ignored);
    return UNITY_END();
}
