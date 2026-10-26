#pragma once

#include <cassert>

using SemaphoreHandle_t = bool*;
inline constexpr unsigned portMAX_DELAY = 0xffffffff;

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new bool(false); }
inline void xSemaphoreTake(SemaphoreHandle_t semaphore, unsigned) {
  assert(!*semaphore);
  *semaphore = true;
}
inline void xSemaphoreGive(SemaphoreHandle_t semaphore) {
  assert(*semaphore);
  *semaphore = false;
}
inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) { delete semaphore; }
