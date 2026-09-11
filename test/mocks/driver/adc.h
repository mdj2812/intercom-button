#pragma once
#include <cstdint>

// ESP-IDF / Arduino-ESP32 ADC types used by audio_recorder.cpp.

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1 = 1,
    ADC_CHANNEL_2 = 2,
    ADC_CHANNEL_3 = 3,
    ADC_CHANNEL_4 = 4,
} adc_channel_t;

typedef adc_channel_t adc1_channel_t;

typedef enum {
    ADC_WIDTH_BIT_12 = 3,
} adc_bits_width_t;

typedef enum {
    ADC_ATTEN_DB_12 = 3,
} adc_atten_t;

inline int _mock_adc_raw = 2048;
inline int _mock_adc_width = 0;
inline int _mock_adc_atten = 0;

inline void mock_adc_reset() {
    _mock_adc_raw = 2048;
    _mock_adc_width = 0;
    _mock_adc_atten = 0;
}

inline void adc1_config_width(adc_bits_width_t width) {
    _mock_adc_width = static_cast<int>(width);
}

inline void adc1_config_channel_atten(adc1_channel_t, adc_atten_t atten) {
    _mock_adc_atten = static_cast<int>(atten);
}

inline int adc1_get_raw(adc1_channel_t) {
    return _mock_adc_raw;
}
