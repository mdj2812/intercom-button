#include <Arduino.h>
#include <LittleFS.h>
#include <unity.h>

/// Verify LittleFS mock works.
void test_littlefs_mount() {
    LittleFS.reset();
    TEST_ASSERT_TRUE(LittleFS.begin(true));
}

void test_littlefs_file_injection() {
    LittleFS.reset();
    LittleFS.begin(true);
    LittleFS.inject("/config.json", R"({"room":"study","wifi_ssid":"test"})");

    File f = LittleFS.open("/config.json", "r");
    TEST_ASSERT_TRUE(f);

    // Read first few bytes
    char buf[64] = {0};
    f.read(reinterpret_cast<uint8_t*>(buf), 63);
    TEST_ASSERT_TRUE(strstr(buf, "\"room\":\"study\"") != nullptr);

    f.close();
    TEST_ASSERT_FALSE(f); // closed
}

void test_littlefs_missing_file() {
    LittleFS.reset();
    LittleFS.begin(true);
    File f = LittleFS.open("/nope.json", "r");
    TEST_ASSERT_FALSE(f); // not found
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_littlefs_mount);
    RUN_TEST(test_littlefs_file_injection);
    RUN_TEST(test_littlefs_missing_file);
    return UNITY_END();
}
