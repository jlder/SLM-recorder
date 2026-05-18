// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/rtc_driver.h
 * @brief Public RTC date/time types and driver API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
} rtc_datetime_t;

/**
 * @brief RTC driver init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_drv_init(void);
/**
 * @brief RTC driver get datetime.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_driver_get_datetime(rtc_datetime_t *out);
/**
 * @brief RTC driver set datetime.
 *
 * Inputs: `in`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_driver_set_datetime(const rtc_datetime_t *in);

