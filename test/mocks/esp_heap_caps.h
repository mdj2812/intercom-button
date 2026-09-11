#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define MALLOC_CAP_SPIRAM (1u << 10)
#define MALLOC_CAP_INTERNAL (1u << 11)
#define MALLOC_CAP_8BIT (1u << 2)

struct multi_heap_info_t {
    size_t total_free_bytes = 0;
};

inline bool _mock_psram_alloc_fail = false;
inline bool _mock_internal_alloc_fail = false;
inline size_t _mock_psram_free_bytes = 2 * 1024 * 1024;

inline void mock_heap_reset() {
    _mock_psram_alloc_fail = false;
    _mock_internal_alloc_fail = false;
    _mock_psram_free_bytes = 2 * 1024 * 1024;
}

inline void* heap_caps_malloc(size_t size, uint32_t caps) {
    if (size == 0)
        return nullptr;
    if ((caps & MALLOC_CAP_SPIRAM) && _mock_psram_alloc_fail)
        return nullptr;
    if ((caps & MALLOC_CAP_INTERNAL) && _mock_internal_alloc_fail)
        return nullptr;
    return std::malloc(size);
}

inline void heap_caps_get_info(multi_heap_info_t* info, uint32_t) {
    if (!info)
        return;
    info->total_free_bytes = _mock_psram_free_bytes;
}
