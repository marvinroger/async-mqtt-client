#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

inline uint32_t currentMillis = 1000;
inline uint32_t millis() { return currentMillis; }

struct IPAddress {};

struct TestEsp {
  uint64_t getEfuseMac() const { return 1; }
  unsigned getChipId() const { return 1; }
  size_t getMaxAllocHeap() const { return 1000000; }
  size_t getMaxFreeBlockSize() const { return 1000000; }
};

inline TestEsp ESP;
