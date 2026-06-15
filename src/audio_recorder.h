#pragma once
#include <cstdint>
#include <cstddef>

/// ESP32-S3 timer-driven ADC audio recorder.
/// Captures mono 16-bit PCM via hw timer ISR, stores in PSRAM.
class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();

    /// Allocate PSRAM buffer, configure ADC & timer.
    /// @param sample_rate  Hz (e.g. 16000)
    /// @param max_secs     maximum recording duration
    bool begin(uint32_t sample_rate, uint32_t max_secs);

    void start();
    uint32_t stop();
    void write_wav_header();

    bool is_recording() const { return _recording; }
    const uint8_t *data()   const { return _buffer_head; }
    size_t total_bytes()    const { return _total_bytes; }
    uint32_t sample_count() const { return _write_idx; }
    uint32_t max_samples()  const { return _max_samples; }

    // ISR-accessible
    volatile uint32_t _write_idx;
    volatile bool     _overflow;
    volatile bool     _recording;
    int16_t          *_buffer;

    static AudioRecorder *_instance;
    void     *_timer_handle;

private:
    static constexpr size_t WAV_HEADER_SIZE = 44;

    uint8_t  *_buffer_head;
    uint32_t  _max_samples;
    uint32_t  _sample_rate;
    size_t    _total_bytes;
    bool      _initialized;
};
