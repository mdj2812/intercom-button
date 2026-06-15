#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unity.h>

/// Minimal ConfigManager-like JSON loading for native tests.
/// This mirrors the logic in config_manager.cpp without Arduino dependencies.

static const char* TEST_FILE = "/tmp/test_config.json";

void setUp() {}
void tearDown() {
    std::remove(TEST_FILE);
}

static void write_json(const char* json) {
    std::ofstream f(TEST_FILE);
    f << json;
    f.close();
}

// ── Tests ───────────────────────────────────────────

void test_load_valid_config() {
    write_json(R"({
        "wifi_ssid": "TestWiFi",
        "wifi_password": "secret123",
        "server_host": "192.168.1.1",
        "server_port": 9999,
        "room": "living",
        "sample_rate": 8000,
        "max_record_secs": 30
    })");

    // Read and parse
    std::ifstream f(TEST_FILE);
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_STRING("TestWiFi", doc["wifi_ssid"]);
    TEST_ASSERT_EQUAL_STRING("secret123", doc["wifi_password"]);
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", doc["server_host"]);
    TEST_ASSERT_EQUAL(9999, doc["server_port"].as<int>());
    TEST_ASSERT_EQUAL_STRING("living", doc["room"]);
    TEST_ASSERT_EQUAL(8000, doc["sample_rate"].as<int>());
    TEST_ASSERT_EQUAL(30, doc["max_record_secs"].as<int>());
}

void test_missing_keys_use_defaults() {
    write_json(R"({"room": "bedroom", "wifi_ssid": "Minimal"})");

    std::ifstream f(TEST_FILE);
    StaticJsonDocument<512> doc;
    deserializeJson(doc, f);
    f.close();

    // Present keys
    TEST_ASSERT_EQUAL_STRING("Minimal", doc["wifi_ssid"]);
    TEST_ASSERT_EQUAL_STRING("bedroom", doc["room"]);

    // Missing keys should return default values
    TEST_ASSERT_FALSE(doc.containsKey("wifi_password"));
    TEST_ASSERT_FALSE(doc.containsKey("server_host"));

    // Our config_manager.cpp handles missing keys gracefully
    // with defaults — verify that pattern works here
    const char* password = doc["wifi_password"] | "default_pass";
    TEST_ASSERT_EQUAL_STRING("default_pass", password);
}

void test_empty_json_does_not_crash() {
    write_json("{}");

    std::ifstream f(TEST_FILE);
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_TRUE(doc.is<JsonObject>());
    TEST_ASSERT_EQUAL(0, doc.size());
}

void test_invalid_json_handled() {
    write_json("{broken");

    std::ifstream f(TEST_FILE);
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    TEST_ASSERT_TRUE(err);
    // Our config_manager.cpp falls back to defaults on parse error
}

// ── WAV header tests ────────────────────────────────

void test_wav_header_structure() {
    // Generate a minimal WAV header for 16000 Hz, 16-bit mono, 1000 samples
    uint32_t sample_rate = 16000;
    uint32_t data_size = 1000 * 2; // 1000 samples × 2 bytes (16-bit)
    uint32_t file_size = 36 + data_size;
    uint16_t block_align = 2;
    uint32_t byte_rate = sample_rate * block_align;

    uint8_t header[44] = {0};

    auto w32 = [&](int off, uint32_t v) { memcpy(header + off, &v, 4); };
    auto w16 = [&](int off, uint16_t v) { memcpy(header + off, &v, 2); };

    memcpy(header, "RIFF", 4);
    w32(4, file_size);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    w32(16, 16);
    w16(20, 1); // PCM
    w16(22, 1); // mono
    w32(24, sample_rate);
    w32(28, byte_rate);
    w16(32, block_align);
    w16(34, 16); // bits per sample
    memcpy(header + 36, "data", 4);
    w32(40, data_size);

    // Verify magic bytes
    TEST_ASSERT_EQUAL_CHAR_ARRAY("RIFF", header, 4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("WAVE", header + 8, 4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("fmt ", header + 12, 4);

    // Verify file size = 36 + data_size
    uint32_t parsed_size;
    memcpy(&parsed_size, header + 4, 4);
    TEST_ASSERT_EQUAL(36 + 2000, parsed_size);

    // Verify sample rate
    uint32_t parsed_sr;
    memcpy(&parsed_sr, header + 24, 4);
    TEST_ASSERT_EQUAL(16000, parsed_sr);

    // Verify data chunk
    TEST_ASSERT_EQUAL_CHAR_ARRAY("data", header + 36, 4);
    uint32_t parsed_data;
    memcpy(&parsed_data, header + 40, 4);
    TEST_ASSERT_EQUAL(2000, parsed_data);

    // Total header size
    TEST_ASSERT_EQUAL(44, sizeof(header));
}

void test_wav_header_sizes() {
    // Different settings should produce correct sizes
    struct {
        uint32_t sr;
        uint32_t samples;
        uint32_t expected_data;
    } cases[] = {
        {8000, 500, 1000},        // 0.5s of 8kHz → 1000 bytes
        {16000, 1000, 2000},      // 1s of 16kHz → 2000 bytes
        {44100, 4410, 8820},      // 0.1s of 44.1kHz → 8820 bytes
        {16000, 960000, 1920000}, // 60s of 16kHz → ~1.92MB
    };

    for (auto& tc : cases) {
        uint32_t data_size = tc.samples * 2;
        TEST_ASSERT_EQUAL(tc.expected_data, data_size);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_load_valid_config);
    RUN_TEST(test_missing_keys_use_defaults);
    RUN_TEST(test_empty_json_does_not_crash);
    RUN_TEST(test_invalid_json_handled);
    RUN_TEST(test_wav_header_structure);
    RUN_TEST(test_wav_header_sizes);
    return UNITY_END();
}
