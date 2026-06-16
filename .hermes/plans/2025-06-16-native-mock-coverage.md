# Native Mock Coverage — Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Make `src/*` code testable on native (x86) by creating Arduino/ESP32 mocks, then add unit tests with real coverage.

**Architecture:** A `test/mocks/` directory provides stub implementations of Arduino, ESP-IDF, and library headers. The native PlatformIO env removes `build_src_filter` and adds mock include paths. Tests live under `test/test_<module>/` and exercise the real src code through the mock layer.

**Tech Stack:** PlatformIO native, Unity test framework, ArduinoJson (already works on native), gcov/lcov.

---

## Module Dependency Map

```
                    main.cpp (state machine)
                   /    |        \         \
                  /     |         \         \
         audio_recorder  |   http_uploader  wifi_manager
                  \      |         /         /
                   \     |        /         /
                    config_manager.cpp
                          |
                    config.h (pin defines, pure)
```

API surface per module:

| Module | Arduino APIs Used | ESP-Specific |
|--------|------------------|--------------|
| `config.h` | None (pure defines) | None |
| `config_manager` | `Serial`, `String`, `LittleFS`, `File` | `LittleFS` |
| `wifi_manager` | `Serial`, `String`, `millis` | `WiFi.mode/begin/status/reconnect/RSSI` |
| `http_uploader` | `Serial`, `String`, `delay` | `WiFi.h`, `HTTPClient` |
| `audio_recorder` | `Serial`, `memset` | `hw_timer_t`, `adc1_*`, `heap_caps_*`, `IRAM_ATTR` |
| `main.cpp` | `Serial`, `digitalRead`, `pinMode`, `delay`, `millis` | `Adafruit_NeoPixel` |

---

## Task 1: Create mock infrastructure (Arduino.h + core types)

**Objective:** Create the minimal mock layer so native tests can `#include <Arduino.h>`

**Files:**
- Create: `test/mocks/Arduino.h`
- Create: `test/mocks/WString.h`
- Create: `test/mocks/Printable.h`
- Create: `test/mocks/Print.h`
- Create: `test/mocks/Stream.h`

**Mock Arduino.h — provides:**

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include "WString.h"

// ── Core types ──────────────────────────────────────
typedef unsigned char uint8_t;
// (already in <cstdint>)

// ── Pin I/O ─────────────────────────────────────────
#define INPUT_PULLUP  0x05
#define LOW   0
#define HIGH  1

inline void pinMode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }
inline int  digitalRead(uint8_t pin) { (void)pin; return HIGH; }
inline void digitalWrite(uint8_t pin, uint8_t val) { (void)pin; (void)val; }

// ── Timing ──────────────────────────────────────────
unsigned long millis();
void delay(unsigned long ms);
unsigned long micros();
void delayMicroseconds(unsigned int us);

// ── Serial mock ─────────────────────────────────────
class SerialMock {
public:
    void begin(unsigned long) {}
    void println(const char* s = "") { printf("%s\n", s ? s : ""); }
    void println(const String& s) { printf("%s\n", s.c_str()); }
    void print(const char* s) { printf("%s", s); }
    int printf(const char* fmt, ...);
    // ... va_list printf
};
extern SerialMock Serial;
```

**Mock WString.h:**

```cpp
#pragma once
#include <string>
#include <cstdio>

class String {
    std::string _s;
public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(const String&) = default;
    
    const char* c_str() const { return _s.c_str(); }
    size_t length() const { return _s.length(); }
    int indexOf(const char* needle) const;
    bool operator==(const String& o) const { return _s == o._s; }
    
    // assignment
    String& operator=(const char* s) { _s = s ? s : ""; return *this; }
};
```

**Step 1:** Create all 5 mock headers in `test/mocks/`
**Step 2:** Update `platformio.ini` native env to add `-Itest/mocks` to build_flags
**Step 3:** Write a smoke test that `#include <Arduino.h>`, calls `Serial.println("hello")`, and compiles
**Step 4:** Run `pio test -e native -v`, verify compilation succeeds
**Step 5:** Commit

---

## Task 2: Mock LittleFS and File

**Objective:** Stub out LittleFS/File so `config_manager.cpp` compiles on native

**Files:**
- Create: `test/mocks/LittleFS.h`
- Create: `test/mocks/FS.h` (File base class)

**Mock LittleFS.h:**

```cpp
#pragma once
#include "FS.h"
#include <string>
#include <map>

class LittleFSClass {
    std::map<std::string, std::string> _files;
    bool _mounted = false;
public:
    bool begin(bool formatOnFail = false);
    File open(const char* path, const char* mode);
    void inject(const char* path, const std::string& content);
    void reset();
};
extern LittleFSClass LittleFS;
```

**Mock FS.h — File class:**

```cpp
#pragma once
#include <string>

class File {
    std::string _content;
    size_t _pos = 0;
    bool _open = false;
public:
    File() = default;
    explicit File(const std::string& content) : _content(content), _open(true) {}
    
    operator bool() const { return _open; }
    int read() { if (_pos >= _content.size()) return -1; return _content[_pos++]; }
    size_t read(uint8_t* buf, size_t len);
    int peek();
    int available();
    void close() { _open = false; _pos = 0; }
};
```

**Step 1:** Create `test/mocks/LittleFS.h` and `test/mocks/FS.h`
**Step 2:** Verify `config_manager.cpp` compiles in native env with mocks
**Step 3:** Commit

---

## Task 3: Mock WiFi and HTTPClient

**Objective:** Stub WiFi + HTTPClient so `wifi_manager.cpp` and `http_uploader.cpp` compile

**Files:**
- Create: `test/mocks/WiFi.h`
- Create: `test/mocks/WiFiType.h`
- Create: `test/mocks/IPAddress.h`

**Mock WiFi.h — minimal:**
- `WiFi.mode()`, `WiFi.begin(ssid, pass)`, `WiFi.status()` → returns mockable enum
- `WiFi.setAutoReconnect()`, `WiFi.reconnect()`, `WiFi.RSSI()`
- All status constants: `WL_CONNECTED`, `WL_DISCONNECTED`, etc.
- Mutable mock state so tests can simulate disconnected/reconnected

**Step 1:** Create WiFi mock headers
**Step 2:** Verify `wifi_manager.cpp` compiles
**Step 3:** Write test: `test_wifi_manager_reconnects_on_disconnect`
**Step 4:** Commit

---

## Task 4: Mock HTTPClient

**Objective:** Stub HTTPClient so `http_uploader.cpp` compiles

**Files:**
- Create: `test/mocks/HTTPClient.h`

**Mock HTTPClient.h:**

```cpp
#pragma once
#include <string>
#include <cstdint>

#define HTTP_CODE_OK 200

class HTTPClient {
public:
    bool begin(const char* url);
    void addHeader(const char* name, const char* value);
    void setTimeout(uint32_t ms);
    int POST(uint8_t* data, size_t len);
    String getString();
    String errorToString(int code);
    void end();
    
    // Test hooks
    static int mock_response_code;
    static std::string mock_response_body;
    static int mock_connect_error;
    static void reset();
};
```

**Step 1:** Create mock HTTPClient header
**Step 2:** Verify `http_uploader.cpp` compiles
**Step 3:** Commit

---

## Task 5: Remove src_filter and fix include paths

**Objective:** Make native env compile all `src/*` files through mocks

**Files:**
- Modify: `platformio.ini` — remove `build_src_filter = -<src/*>`, add mock include path
- Modify: Possibly add `src/` files that can't be mocked yet to exclude list

**platformio.ini changes:**
```ini
build_flags =
    -std=c++17
    -O0
    -g
    --coverage
    -Itest/mocks
build_unflags = -Os -O2
build_src_filter = -<src/main.cpp>   # exclude main for now (depends on NeoPixel)
```

**Step 1:** Update platformio.ini
**Step 2:** Try `pio test -e native -v` — expect main.cpp to fail, exclude it
**Step 3:** Verify all other modules compile
**Step 4:** Commit

---

## Task 6: Test config_manager (real coverage!)

**Objective:** Replace the duplicated logic in `test_config` with tests that call the real `ConfigManager`

**Files:**
- Create: `test/test_config_manager/test_main.cpp` (rename approach)

**Step 1:** Write test that:
- Pre-injects JSON into mock LittleFS (`LittleFS.inject("/config.json", "{...}")`)
- Calls `ConfigManager::begin()`
- Verifies `ConfigManager::room_target()` returns correct value
- Verifies defaults when JSON is missing

**Step 2:** Run `make test-coverage` — expect real coverage on `config_manager.cpp`

**Step 3:** Commit

---

## Task 7: Test wifi_manager state machine

**Objective:** Test WiFiManager reconnection logic

**Files:**
- Create: `test/test_wifi_manager/test_main.cpp`

**Test cases:**
1. `begin` + `update` when connected → returns true
2. `update` when disconnected → starts reconnect attempt
3. `status_str` returns correct strings for each WL_* constant
4. `rssi` returns 0 when disconnected

**Step 1:** Write tests
**Step 2:** Verify coverage
**Step 3:** Commit

---

## Task 8: Test http_uploader retry logic

**Objective:** Test HTTPUploader retry and error handling

**Files:**
- Create: `test/test_http_uploader/test_main.cpp`

**Test cases:**
1. Successful upload (HTTP 200 + `{"ok":true}`) → returns true
2. HTTP error code → retries 3 times
3. Connection error (< 0) → still returns true (audio delivery priority)
4. Empty data → returns false
5. URL construction includes room target

**Step 1:** Write tests using mock HTTPClient state injection
**Step 2:** Verify coverage
**Step 3:** Commit

---

## Task 9: Test WAV header generation

**Objective:** Move WAV header test to use real `AudioRecorder` class (header logic is pure C++)

**Files:**
- Create: `test/test_audio_recorder/test_main.cpp`

**Approach:** The WAV header writing in `AudioRecorder::write_wav_header()` is pure pointer math + memcpy — no hardware deps. We can test it by:
1. Allocating a buffer manually
2. Setting `_write_idx`, `_sample_rate`
3. Calling `write_wav_header()` (make it public or friend)
4. Verifying RIFF/WAVE/fmt/data chunks

**Step 1:** Create test
**Step 2:** Verify coverage on `write_wav_header` from real `audio_recorder.cpp`
**Step 3:** Commit

---

## Task 10: CI integration

**Objective:** Verify the full coverage pipeline works end-to-end

**Files:**
- No changes needed — CI coverage job already exists

**Step 1:** Push branch, open PR
**Step 2:** Verify CI `coverage` job passes with real src coverage
**Step 3:** Check artifact HTML report shows real line coverage across src/ modules

---

## Task 11: Cleanup — remove old test_config

**Objective:** Remove the duplicated logic in `test/test_config/test_main.cpp`

**Files:**
- Remove: `test/test_config/test_main.cpp` (replaced by test_config_manager)

**Step 1:** Delete old test file
**Step 2:** Verify `make test` still passes all module-specific tests
**Step 3:** Commit

---

## Expected Final Coverage

| Module | Lines | Notes |
|--------|-------|-------|
| `config_manager.cpp` | ~80% | JSON + LittleFS binding; File I/O mocked |
| `wifi_manager.cpp` | ~90% | State machine logic is testable; WiFi.h calls are mock pass-throughs |
| `http_uploader.cpp` | ~85% | Retry + URL construction; HTTPClient calls mocked |
| `audio_recorder.cpp` | ~50% | WAV header testable; ISR/hardware cannot be tested |
| `main.cpp` | 0% | Excluded (state machine is pure hardware I/O + NeoPixel) |

---

## Architecture Decision: Header-only mocks

All mocks in `test/mocks/` are **header-only** (`.h` with inline implementations). 
This avoids:
- Separate mock compilation in SCons
- Linker ordering issues
- `.cpp` files that PlatformIO might not pick up

Trade-off: mocks recompile every time, but the mock headers are tiny (< 100 lines each).

## Dependency Order

```
Task 1 (Arduino.h) 
  → Task 2 (LittleFS) 
    → Task 3 (WiFi) + Task 4 (HTTPClient)
      → Task 5 (remove src_filter)
        → Tasks 6-9 (tests, can run in parallel)
          → Task 10 (CI verification)
            → Task 11 (cleanup)
```

Tasks 6-9 are test-writing tasks that compile successfully once Tasks 1-5 are done. They can be parallelized.
