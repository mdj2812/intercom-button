/// Native tests for src/main.cpp setup()/loop() (hardware mocked).

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <unity.h>

#include "../../src/main.cpp"

static const char* CONFIG_JSON = R"({
    "wifi_ssid": "test-wifi",
    "wifi_password": "secret",
    "server_scheme": "http",
    "server_host": "ha.local",
    "server_port": 8123,
    "pins": [4, 5, 12, 13]
})";

static const char* HELLO_OK = R"({
    "status": "ok",
    "device_name": "HIL",
    "sample_rate": 16000,
    "max_record_secs": 60,
    "buttons": {"4": "study"}
})";

static bool g_booted = false;

static void boot() {
    if (g_booted)
        return;
    LittleFS.reset();
    LittleFS.inject("/config.json", CONFIG_JSON);
    Preferences::reset_all();
    mock_wifi_reset();
    mock_http_reset();
    mock_heap_reset();
    mock_timer_reset();
    mock_adc_reset();
    mock_esp_reset();
    mock_set_millis(0);
    Serial.mock_clear_rx();
    mock_wifi_set_connected(true);
    mock_http_set_response(200, HELLO_OK);
    setup();
    g_booted = true;
}

void setUp() {}
void tearDown() {}

void test_setup_completes_without_restart() {
    boot();
    TEST_ASSERT_EQUAL(0, _mock_esp_restart_count);
    TEST_ASSERT_TRUE(led.began);
}

void test_idle_hello_does_not_start_ota() {
    boot();
    for (int i = 0; i < 8; i++)
        loop();
    TEST_ASSERT_EQUAL(0, _mock_esp_restart_count);
    TEST_ASSERT_GREATER_THAN(0, _http_mock.post_call_count);
}

void test_confirm_test_dry_run_times_out_to_idle() {
    boot();
    for (int i = 0; i < 4; i++)
        loop();
    Serial.mock_push("confirm_test\n");
    loop();
    mock_set_millis(millis() + OTAManager::CONFIRM_DRY_RUN_SEC * 1000UL + 50);
    for (int i = 0; i < 4; i++)
        loop();
    TEST_ASSERT_EQUAL(0, _mock_esp_restart_count);
    {
        Preferences prefs;
        TEST_ASSERT_TRUE(prefs.begin("ota", true));
        TEST_ASSERT_EQUAL(0, (int) prefs.getUInt("fail_count", 0));
        TEST_ASSERT_FALSE(prefs.getBool("pending", false));
    }
}

void test_serial_ota_failed_download_stays_alive() {
    boot();
    for (int i = 0; i < 4; i++)
        loop();
    Serial.mock_push("ota\n");
    loop();
    TEST_ASSERT_EQUAL(0, _mock_esp_restart_count);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_setup_completes_without_restart);
    RUN_TEST(test_idle_hello_does_not_start_ota);
    RUN_TEST(test_confirm_test_dry_run_times_out_to_idle);
    RUN_TEST(test_serial_ota_failed_download_stays_alive);
    return UNITY_END();
}
