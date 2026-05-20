// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/rtc_driver.cpp
 * @brief RTC driver wrapper with sanity checks and read/write verification.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/drivers/rtc_driver.h"
#include <Arduino.h>
#include <Wire.h>
#include "src/board/pin_config.h"
#include "config.h"
#include "SensorPCF85063.hpp"


static SensorPCF85063 s_rtc;

/**
 * RTC sanity check performs the rtc driver operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `t`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool rtc_sanity_check(const rtc_datetime_t& t) {
  // PCF85063 typically holds years 2000-2099. Use a slightly wider bound to
  // avoid false negatives on boards with unset RTC.
  if(t.year < 2000u || t.year > 2099u) return false;
  if(t.month < 1u || t.month > 12u) return false;
  if(t.day < 1u || t.day > 31u) return false;
  if(t.hour > 23u) return false;
  if(t.min > 59u) return false;
  if(t.sec > 59u) return false;
  return true;
}

/**
 * Convert date/time fields to a deterministic second count.
 *
 * This helper does not validate calendar correctness. Its only purpose is to compare an RTC value written by this driver
 * with the value read back immediately after the write.
 *
 * The month transform is March-based:
 * - March becomes month 1;
 * - January/February become months 11/12 of the previous year.
 * This lets the day offset from March 1st be calculated with one compact equation while leap-year effects remain in the year term.
 *
 * Inputs: `dt`.
 * Returns: Deterministic second count from the formula epoch.
 */
static int64_t rtc_datetime_to_seconds_(const rtc_datetime_t& dt) {
  int64_t y = (int64_t)dt.year;
  int64_t m = (int64_t)dt.month;

  if(m >= 3){
    m -= 2;       // March = 1
  } else {
    m += 10;      // January/February become months 11/12 of previous year
    y -= 1;
  }

  const int64_t d_months = ((153 * (m - 1)) + 2) / 5;
  const int64_t d_years = (365 * y) + (y / 4) - (y / 100) + (y / 400);

  return ((d_years + d_months + ((int64_t)dt.day - 1) + 60) * 86400) +
         ((int64_t)dt.hour * 3600) + ((int64_t)dt.min * 60) + (int64_t)dt.sec;
}

/**
 * Compare RTC set/readback values allowing one second of elapsed time.
 *
 * Inputs: `set_time`, `readback`.
 * Returns: `true` if readback equals set_time or set_time + 1 second.
 */
static bool rtc_equal_allowing_1s(const rtc_datetime_t& set_time, const rtc_datetime_t& readback) {
  const int64_t s_set = rtc_datetime_to_seconds_(set_time);
  const int64_t s_read = rtc_datetime_to_seconds_(readback);

  return (s_read == s_set) || (s_read == (s_set + 1));
}

/**
 * Initializes rtc drv init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_drv_init(void) {
  // Prototype: rtc.begin(Wire, IIC_SDA, IIC_SCL);
  s_rtc.begin(Wire, IIC_SDA, IIC_SCL);
  // The library does not return a status for begin(), so we perform a bounded
  // post-condition check by reading back the current datetime and validating
  // that it is within representable ranges.
  rtc_datetime_t now = {};
  (void)rtc_driver_get_datetime(&now);
  const bool ok = rtc_sanity_check(now);
  if(ok){
  } else {
  }
  return ok;
}

/**
 * Reads rtc driver get datetime from the underlying hardware or cached source
 * and reports whether the value is valid.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_driver_get_datetime(rtc_datetime_t *out) {
  if (!out) return false;

  for (uint8_t attempt = 0; attempt < (uint8_t)RTC_READ_MAX_RETRIES; ++attempt) {
    const RTC_DateTime dt0 = s_rtc.getDateTime();
    const RTC_DateTime dt1 = s_rtc.getDateTime();

    const uint32_t t0 = (uint32_t)dt0.getHour() * 3600u + (uint32_t)dt0.getMinute() * 60u + (uint32_t)dt0.getSecond();
    uint32_t t1 = (uint32_t)dt1.getHour() * 3600u + (uint32_t)dt1.getMinute() * 60u + (uint32_t)dt1.getSecond();

    if (t1 < t0) {
      t1 += 86400u;
    }

    if ((t1 - t0) <= 1u) {
      out->year  = (uint16_t)dt1.getYear();
      out->month = (uint8_t)dt1.getMonth();
      out->day   = (uint8_t)dt1.getDay();
      out->hour  = (uint8_t)dt1.getHour();
      out->min   = (uint8_t)dt1.getMinute();
      out->sec   = (uint8_t)dt1.getSecond();
      return true;
    }
  }
  return false;
}

/**
 * Updates rtc driver set datetime state and applies the change to the owning
 * module or hardware interface.
 *
 * Inputs: `in`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_driver_set_datetime(const rtc_datetime_t *in) {
  if (!in) return false;

  s_rtc.setDateTime((int)in->year, (int)in->month, (int)in->day,
                    (int)in->hour, (int)in->min, (int)in->sec);

  // Post-condition check: read back and compare (allowing 1s of latency).
  rtc_datetime_t rb = {};
  if(!rtc_driver_get_datetime(&rb)){
    return false;
  }
  if(!rtc_sanity_check(rb)){
    return false;
  }
  return rtc_equal_allowing_1s(*in, rb);
}
