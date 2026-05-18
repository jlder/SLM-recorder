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
 * RTC equal allowing 1s performs the rtc driver operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `a`, `b`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool rtc_equal_allowing_1s(const rtc_datetime_t& a, const rtc_datetime_t& b) {
  if(a.year != b.year) return false;
  if(a.month != b.month) return false;
  if(a.day != b.day) return false;
  if(a.hour != b.hour) return false;
  if(a.min != b.min) return false;
  // Allow 1 second drift due to readback latency.
  const int ds = (int)a.sec - (int)b.sec;
  return (ds == 0) || (ds == 1) || (ds == -1);
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
