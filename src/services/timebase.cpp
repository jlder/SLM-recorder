// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/timebase.cpp
 * @brief Recording timebase helper based on RTC time-of-day and monotonic elapsed time.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/timebase.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include "esp_timer.h"

static uint32_t s_recording_time_start_ms = 0;
static uint32_t s_esp_start_ms = 0;
static bool     s_record_marked = false;
static char     s_token[24];

/**
 * Initializes timebase init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void timebase_init(void) {
  s_recording_time_start_ms = 0;
  s_esp_start_ms = 0;
  s_record_marked = false;
  s_token[0] = '\0';
}

/**
 * Maintains or queries the recording timebase derived from the captured RTC
 * start time and monotonic ESP timer.
 *
 * Inputs: `dt`.
 * Returns: Requested numeric value.
 */
static uint32_t rtc_time_of_day_ms(const rtc_datetime_t *dt) {
  return (uint32_t)dt->hour * 3600000UL +
         (uint32_t)dt->min  *   60000UL +
         (uint32_t)dt->sec  *    1000UL;
}

/**
 * Maintains or queries the recording timebase derived from the captured RTC
 * start time and monotonic ESP timer.
 *
 * Inputs: `dt`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool timebase_mark_record_start(const rtc_datetime_t *dt) {
  if (!dt) return false;

  const uint32_t start_ms = rtc_time_of_day_ms(dt);
  const uint32_t esp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

  const int n = snprintf(s_token, sizeof(s_token),
                         "%04u%02u%02u_%02u%02u%02u",
                         (unsigned)dt->year, (unsigned)dt->month, (unsigned)dt->day,
                         (unsigned)dt->hour, (unsigned)dt->min, (unsigned)dt->sec);

  if ((n < 0) || ((size_t)n >= sizeof(s_token))) {
    s_recording_time_start_ms = 0;
    s_esp_start_ms = 0;
    s_record_marked = false;
    s_token[0] = '\0';
    return false;
  }

  s_recording_time_start_ms = start_ms;
  s_esp_start_ms = esp_ms;
  s_record_marked = true;
  return true;
}

/**
 * Returns the requested timebase get ms since midnight information from the
 * module state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint32_t timebase_get_ms_since_midnight(void) {
  if (!s_record_marked) {
    return 0u;
  }

  if (s_token[0] == '\0') {
    // Timestamp is only meaningful after record start (mapping captured).
    return 0u;
  }

  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
  int64_t v = (int64_t)s_recording_time_start_ms - (int64_t)s_esp_start_ms + (int64_t)now_ms;
  int64_t mod = v % 86400000LL;
  if (mod < 0) mod += 86400000LL;
  return (uint32_t)mod;
}

/**
 * @brief Return the compact date/time token for the current recording start.
 *
 * Inputs: None.
 * Returns: Pointer to the internal compact date/time string.
 */
const char *timebase_get_datetime_compact(void) {
  return s_token;
}
