#pragma once
#include <Arduino.h>

// Serial logging with a tag and a level. LOG_LEVEL (build flag) picks how
// much: 0 errors, 1 + warnings, 2 + info (default), 3 + debug.
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

#define LOG_AT(lvl, tag, fmt, ...) \
  do { if (LOG_LEVEL >= (lvl)) Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__); } while (0)
#define LOGE(tag, fmt, ...) LOG_AT(0, tag, "ERROR " fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) LOG_AT(1, tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) LOG_AT(2, tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) LOG_AT(3, tag, fmt, ##__VA_ARGS__)
