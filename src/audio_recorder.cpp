#include "audio_recorder.h"
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_heap_caps.h>
#include <cstring>

static const char *TAG = "audio";

AudioRecorder *AudioRecorder::_instance = nullptr;

// ── ISR trampoline ──────────────────────────────────
// Arduino-ESP32 timer ISR signature: void (*)(void)
// _instance must be set before enabling the timer.
static void IRAM_ATTR isr_trampoline() {
    auto *rec = AudioRecorder::_instance;
    if (!rec || !rec->_recording) return;

    if (rec->_write_idx >= rec->max_samples()) {
        rec->_overflow = true;
        timerAlarmDisable((hw_timer_t *)rec->_timer_handle);
        rec->_recording = false;
        return;
    }

    int raw = adc1_get_raw((adc1_channel_t)PIN_ADC_CHANNEL);
    rec->_buffer[rec->_write_idx++] = (int16_t)((raw - 2048) << 4);
}

// ── Constructor / Destructor ────────────────────────

AudioRecorder::AudioRecorder()
    : _write_idx(0), _buffer(nullptr), _buffer_head(nullptr),
      _max_samples(0), _total_bytes(0), _recording(false),
      _initialized(false), _timer_handle(nullptr), _overflow(false) {}

AudioRecorder::~AudioRecorder() {
    if (_recording) stop();
    if (_timer_handle) {
        timerEnd((hw_timer_t *)_timer_handle);
    }
    free(_buffer_head);
    if (_instance == this) _instance = nullptr;
}

// ── begin() ─────────────────────────────────────────

bool AudioRecorder::begin() {
    _instance = this;

    _max_samples = SAMPLE_RATE * MAX_RECORD_SECS;
    size_t pcm_bytes = _max_samples * sizeof(int16_t);
    size_t total_alloc = WAV_HEADER_SIZE + pcm_bytes;

    _buffer_head = (uint8_t *)heap_caps_malloc(total_alloc,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_buffer_head) {
        Serial.printf("[%s] FATAL: PSRAM alloc(%u) failed. "
                      "Trying DRAM fallback...\n", TAG, total_alloc);
        _buffer_head = (uint8_t *)heap_caps_malloc(total_alloc,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!_buffer_head) {
            Serial.printf("[%s] DRAM fallback also failed\n", TAG);
            return false;
        }
    }
    _buffer = (int16_t *)(_buffer_head + WAV_HEADER_SIZE);
    memset(_buffer_head, 0, total_alloc);
    _total_bytes = 0;
    _write_idx = 0;

    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    Serial.printf("[%s] Buffer: %u samples (%u sec), PSRAM free: %u KB\n",
                  TAG, _max_samples, MAX_RECORD_SECS,
                  info.total_free_bytes / 1024);

    // ADC config
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten((adc1_channel_t)PIN_ADC_CHANNEL, ADC_ATTEN_DB_12);

    // Timer: timer 0, 80-divider (1 MHz tick), count-up
    _timer_handle = timerBegin(0, 80, true);
    if (!_timer_handle) {
        Serial.printf("[%s] FATAL: timerBegin failed\n", TAG);
        return false;
    }
    timerAttachInterrupt((hw_timer_t *)_timer_handle, &isr_trampoline, true);
    timerAlarmWrite((hw_timer_t *)_timer_handle,
                    1000000 / SAMPLE_RATE, // µs
                    true);                 // auto-reload

    _initialized = true;
    Serial.printf("[%s] Ready — ISR @ %d Hz\n", TAG, SAMPLE_RATE);
    return true;
}

// ── start() / stop() ────────────────────────────────

void AudioRecorder::start() {
    if (!_initialized || _recording) return;
    _write_idx = 0;
    _overflow = false;
    _total_bytes = 0;
    _recording = true;
    timerAlarmEnable((hw_timer_t *)_timer_handle);
    Serial.printf("[%s] Recording started\n", TAG);
}

uint32_t AudioRecorder::stop() {
    if (!_recording) return _write_idx;
    _recording = false;
    timerAlarmDisable((hw_timer_t *)_timer_handle);
    Serial.printf("[%s] Stopped — %u samples (%.1f s)%s\n",
                  TAG, _write_idx, (float)_write_idx / SAMPLE_RATE,
                  _overflow ? " [OVERFLOW]" : "");
    return _write_idx;
}

// ── write_wav_header() ──────────────────────────────

void AudioRecorder::write_wav_header() {
    uint32_t data_size   = _write_idx * sizeof(int16_t);
    uint32_t file_size   = 36 + data_size;
    uint16_t block_align = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    uint32_t byte_rate   = SAMPLE_RATE * block_align;

    auto w32 = [&](int off, uint32_t v) { memcpy(_buffer_head + off, &v, 4); };
    auto w16 = [&](int off, uint16_t v) { memcpy(_buffer_head + off, &v, 2); };

    memcpy(_buffer_head,      "RIFF", 4);
    w32(4,  file_size);
    memcpy(_buffer_head + 8,  "WAVE", 4);
    memcpy(_buffer_head + 12, "fmt ", 4);
    w32(16, 16);             // fmt chunk size
    w16(20, 1);              // PCM format
    w16(22, NUM_CHANNELS);
    w32(24, SAMPLE_RATE);
    w32(28, byte_rate);
    w16(32, block_align);
    w16(34, BITS_PER_SAMPLE);
    memcpy(_buffer_head + 36, "data", 4);
    w32(40, data_size);

    _total_bytes = WAV_HEADER_SIZE + data_size;
    Serial.printf("[%s] WAV header written — %u bytes\n", TAG, _total_bytes);
}
