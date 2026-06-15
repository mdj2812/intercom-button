#include "audio_recorder.h"
#include "config.h" // PIN_ADC_CHANNEL
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_heap_caps.h>
#include <cstring>

static const char* TAG = "audio";

AudioRecorder* AudioRecorder::_instance = nullptr;

// ── ISR trampoline ──────────────────────────────────
static void IRAM_ATTR isr_trampoline() {
    auto* rec = AudioRecorder::_instance;
    if (!rec || !rec->_recording)
        return;

    if (rec->_write_idx >= rec->max_samples()) {
        rec->_overflow = true;
        timerAlarmDisable((hw_timer_t*) rec->_timer_handle);
        rec->_recording = false;
        return;
    }

    int raw = adc1_get_raw((adc1_channel_t) PIN_ADC_CHANNEL);
    rec->_buffer[rec->_write_idx++] = (int16_t) ((raw - 2048) << 4);
}

// ── Constructor / Destructor ────────────────────────

AudioRecorder::AudioRecorder()
    : _write_idx(0), _buffer(nullptr), _buffer_head(nullptr), _max_samples(0), _sample_rate(0), _total_bytes(0),
      _recording(false), _initialized(false), _timer_handle(nullptr), _overflow(false) {}

AudioRecorder::~AudioRecorder() {
    if (_recording)
        stop();
    if (_timer_handle)
        timerEnd((hw_timer_t*) _timer_handle);
    free(_buffer_head);
    if (_instance == this)
        _instance = nullptr;
}

// ── begin() ─────────────────────────────────────────

bool AudioRecorder::begin(uint32_t sample_rate, uint32_t max_secs) {
    _instance = this;
    _sample_rate = sample_rate;

    _max_samples = sample_rate * max_secs;
    size_t pcm_bytes = _max_samples * sizeof(int16_t);
    size_t total_alloc = WAV_HEADER_SIZE + pcm_bytes;

    _buffer_head = (uint8_t*) heap_caps_malloc(total_alloc, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_buffer_head) {
        _buffer_head = (uint8_t*) heap_caps_malloc(total_alloc, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!_buffer_head) {
            Serial.printf("[%s] FATAL: alloc(%u) failed\n", TAG, total_alloc);
            return false;
        }
    }
    _buffer = (int16_t*) (_buffer_head + WAV_HEADER_SIZE);
    memset(_buffer_head, 0, total_alloc);
    _total_bytes = 0;
    _write_idx = 0;

    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    Serial.printf("[%s] Buffer: %u samples (%u sec), PSRAM free: %u KB\n", TAG, _max_samples, max_secs,
                  info.total_free_bytes / 1024);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten((adc1_channel_t) PIN_ADC_CHANNEL, ADC_ATTEN_DB_12);

    _timer_handle = timerBegin(0, 80, true);
    if (!_timer_handle) {
        Serial.printf("[%s] FATAL: timerBegin failed\n", TAG);
        return false;
    }
    timerAttachInterrupt((hw_timer_t*) _timer_handle, &isr_trampoline, true);
    timerAlarmWrite((hw_timer_t*) _timer_handle, 1000000 / _sample_rate, true);

    _initialized = true;
    Serial.printf("[%s] Ready — ISR @ %u Hz\n", TAG, _sample_rate);
    return true;
}

// ── start() / stop() ────────────────────────────────

void AudioRecorder::start() {
    if (!_initialized || _recording)
        return;
    _write_idx = 0;
    _overflow = false;
    _total_bytes = 0;
    _recording = true;
    timerAlarmEnable((hw_timer_t*) _timer_handle);
    Serial.printf("[%s] Recording started\n", TAG);
}

uint32_t AudioRecorder::stop() {
    if (!_recording)
        return _write_idx;
    _recording = false;
    timerAlarmDisable((hw_timer_t*) _timer_handle);
    Serial.printf("[%s] Stopped — %u samples (%.1f s)%s\n", TAG, _write_idx, (float) _write_idx / _sample_rate,
                  _overflow ? " [OVERFLOW]" : "");
    return _write_idx;
}

// ── write_wav_header() ──────────────────────────────

void AudioRecorder::write_wav_header() {
    uint32_t data_size = _write_idx * sizeof(int16_t);
    uint32_t file_size = 36 + data_size;
    uint16_t block_align = 1 * (16 / 8); // mono 16-bit
    uint32_t byte_rate = _sample_rate * block_align;

    auto w32 = [&](int off, uint32_t v) { memcpy(_buffer_head + off, &v, 4); };
    auto w16 = [&](int off, uint16_t v) { memcpy(_buffer_head + off, &v, 2); };

    memcpy(_buffer_head, "RIFF", 4);
    w32(4, file_size);
    memcpy(_buffer_head + 8, "WAVE", 4);
    memcpy(_buffer_head + 12, "fmt ", 4);
    w32(16, 16);
    w16(20, 1); // PCM
    w16(22, 1); // mono
    w32(24, _sample_rate);
    w32(28, byte_rate);
    w16(32, block_align);
    w16(34, 16); // bits per sample
    memcpy(_buffer_head + 36, "data", 4);
    w32(40, data_size);

    _total_bytes = WAV_HEADER_SIZE + data_size;
}
