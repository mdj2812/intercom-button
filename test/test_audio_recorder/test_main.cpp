/// Native tests for AudioRecorder (timer ISR, ADC, PSRAM heap mocks).

#include "audio_recorder.h"
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <unity.h>

static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

static uint16_t le16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

void setUp() {
    mock_heap_reset();
    mock_timer_reset();
    mock_adc_reset();
    mock_set_millis(0);
}

void tearDown() {}

void test_begin_allocates_and_arms_timer() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(8000, 1));
    TEST_ASSERT_EQUAL_UINT32(8000, rec.max_samples());
    TEST_ASSERT_FALSE(rec.is_recording());
    TEST_ASSERT_NOT_NULL(rec.data());
    TEST_ASSERT_EQUAL(ADC_WIDTH_BIT_12, _mock_adc_width);
    TEST_ASSERT_NOT_NULL(_mock_hw_timer.isr);
}

void test_begin_falls_back_to_internal_when_psram_fails() {
    _mock_psram_alloc_fail = true;
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(8000, 1));
    TEST_ASSERT_NOT_NULL(rec.data());
}

void test_begin_fails_when_heap_exhausted() {
    _mock_psram_alloc_fail = true;
    _mock_internal_alloc_fail = true;
    AudioRecorder rec;
    TEST_ASSERT_FALSE(rec.begin(8000, 1));
}

void test_begin_fails_when_timer_begin_fails() {
    _mock_timer_begin_fail = true;
    AudioRecorder rec;
    TEST_ASSERT_FALSE(rec.begin(8000, 1));
}

void test_configure_rejects_zero() {
    AudioRecorder rec;
    TEST_ASSERT_FALSE(rec.configure(0, 1));
    TEST_ASSERT_FALSE(rec.configure(8000, 0));
}

void test_configure_noop_when_unchanged() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(8000, 1));
    const uint8_t* first = rec.data();
    TEST_ASSERT_TRUE(rec.configure(8000, 1));
    TEST_ASSERT_TRUE(first == rec.data());
}

void test_configure_reallocates_on_rate_change() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(8000, 1));
    TEST_ASSERT_TRUE(rec.configure(16000, 1));
    TEST_ASSERT_EQUAL_UINT32(16000, rec.max_samples());
}

void test_configure_rejected_while_recording() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(8000, 1));
    rec.start();
    TEST_ASSERT_FALSE(rec.configure(16000, 1));
    rec.stop();
}

void test_start_stop_and_isr_samples() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(100, 1));
    rec.start();
    TEST_ASSERT_TRUE(rec.is_recording());
    TEST_ASSERT_TRUE(_mock_hw_timer.alarm_enabled);

    _mock_adc_raw = 2048 + 1;
    mock_timer_isr();
    mock_timer_isr();
    TEST_ASSERT_EQUAL_UINT32(2, rec.sample_count());
    TEST_ASSERT_EQUAL_INT16(16, rec._buffer[0]);
    TEST_ASSERT_EQUAL_INT16(16, rec._buffer[1]);

    TEST_ASSERT_EQUAL_UINT32(2, rec.stop());
    TEST_ASSERT_FALSE(rec.is_recording());
    TEST_ASSERT_FALSE(_mock_hw_timer.alarm_enabled);
}

void test_isr_ignored_when_not_recording() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(100, 1));
    mock_timer_isr();
    TEST_ASSERT_EQUAL_UINT32(0, rec.sample_count());
}

void test_isr_overflow_stops_recording() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(8, 1));
    rec.start();
    for (int i = 0; i < 8; i++)
        mock_timer_isr();
    TEST_ASSERT_EQUAL_UINT32(8, rec.sample_count());
    TEST_ASSERT_TRUE(rec.is_recording());
    mock_timer_isr();
    TEST_ASSERT_FALSE(rec.is_recording());
    TEST_ASSERT_TRUE(rec._overflow);
    TEST_ASSERT_FALSE(_mock_hw_timer.alarm_enabled);
}

void test_wav_header_pcm_mono_16bit() {
    AudioRecorder rec;
    TEST_ASSERT_TRUE(rec.begin(16000, 1));
    rec.start();
    for (int i = 0; i < 10; i++)
        mock_timer_isr();
    rec.stop();
    rec.write_wav_header();

    const uint8_t* wav = rec.data();
    TEST_ASSERT_EQUAL(44 + 10 * 2, rec.total_bytes());
    TEST_ASSERT_EQUAL_INT(0, memcmp(wav, "RIFF", 4));
    TEST_ASSERT_EQUAL_INT(0, memcmp(wav + 8, "WAVE", 4));
    TEST_ASSERT_EQUAL_INT(0, memcmp(wav + 12, "fmt ", 4));
    TEST_ASSERT_EQUAL_INT(0, memcmp(wav + 36, "data", 4));
    TEST_ASSERT_EQUAL_UINT32(36 + 20, le32(wav + 4));
    TEST_ASSERT_EQUAL_UINT16(1, le16(wav + 20));
    TEST_ASSERT_EQUAL_UINT16(1, le16(wav + 22));
    TEST_ASSERT_EQUAL_UINT32(16000, le32(wav + 24));
    TEST_ASSERT_EQUAL_UINT32(32000, le32(wav + 28));
    TEST_ASSERT_EQUAL_UINT16(2, le16(wav + 32));
    TEST_ASSERT_EQUAL_UINT16(16, le16(wav + 34));
    TEST_ASSERT_EQUAL_UINT32(20, le32(wav + 40));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_allocates_and_arms_timer);
    RUN_TEST(test_begin_falls_back_to_internal_when_psram_fails);
    RUN_TEST(test_begin_fails_when_heap_exhausted);
    RUN_TEST(test_begin_fails_when_timer_begin_fails);
    RUN_TEST(test_configure_rejects_zero);
    RUN_TEST(test_configure_noop_when_unchanged);
    RUN_TEST(test_configure_reallocates_on_rate_change);
    RUN_TEST(test_configure_rejected_while_recording);
    RUN_TEST(test_start_stop_and_isr_samples);
    RUN_TEST(test_isr_ignored_when_not_recording);
    RUN_TEST(test_isr_overflow_stops_recording);
    RUN_TEST(test_wav_header_pcm_mono_16bit);
    return UNITY_END();
}
