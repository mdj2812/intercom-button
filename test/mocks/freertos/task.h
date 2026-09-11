#pragma once
#include "FreeRTOS.h"

using TaskFunction_t = void (*)(void*);
using TaskHandle_t = void*;

inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char*, uint32_t, void*, UBaseType_t, TaskHandle_t*,
                                          BaseType_t) {
    return 1;
}

inline void vTaskDelay(TickType_t) {}
