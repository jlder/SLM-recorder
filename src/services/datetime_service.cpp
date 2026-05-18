// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/datetime_service.cpp
 * @brief Shared application date/time cache synchronized with RTC hardware.
 *
 * @details
 * The cache allows UI/support code and recorder-core code to use one coherent
 * application date/time value. RTC hardware access remains explicit and is
 * performed only by synchronization functions.
 */

#include "src/services/datetime_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_datetime_lock = portMUX_INITIALIZER_UNLOCKED;

static rtc_datetime_t s_datetime = {};
static bool s_datetime_valid = false;
static bool s_rtc_update_pending = false;

/**
 * Initialize the date/time cache state.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
void datetime_service_init(void){
  taskENTER_CRITICAL(&s_datetime_lock);
  s_datetime = {};
  s_datetime_valid = false;
  s_rtc_update_pending = false;
  taskEXIT_CRITICAL(&s_datetime_lock);
}

/**
 * Copy the current cached date/time value.
 *
 * Parameters:
 *   out - destination for the cached date/time.
 *
 * Return:
 *   true if a valid cached date/time was copied,
 *   false if the cache is invalid or out is null.
 */
bool datetime_service_get(rtc_datetime_t *out){
  if(out == nullptr){
    return false;
  }

  bool valid = false;

  taskENTER_CRITICAL(&s_datetime_lock);
  valid = s_datetime_valid;
  if(valid){
    *out = s_datetime;
  }
  taskEXIT_CRITICAL(&s_datetime_lock);

  return valid;
}

/**
 * Update the cached date/time value and request RTC hardware synchronization.
 *
 * Parameters:
 *   in - complete date/time value to store in the cache.
 *
 * Return:
 *   true if the cached value was updated,
 *   false if in is null.
 */
bool datetime_service_set(const rtc_datetime_t *in){
  if(in == nullptr){
    return false;
  }

  taskENTER_CRITICAL(&s_datetime_lock);
  s_datetime = *in;
  s_datetime_valid = true;
  s_rtc_update_pending = true;
  taskEXIT_CRITICAL(&s_datetime_lock);

  return true;
}

/**
 * Synchronize the cached date/time value with RTC hardware.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if synchronization succeeded,
 *   false otherwise.
 *
 * Behavior:
 *   - if a local update is pending, write the cached value to RTC;
 *   - otherwise, refresh the cache from RTC.
 */
bool datetime_service_sync_rtc(void){
  rtc_datetime_t dt = {};
  bool valid = false;
  bool rtc_update_pending = false;

  // Copy cache state under lock, but do not perform slow RTC I/O while locked.
  taskENTER_CRITICAL(&s_datetime_lock);
  dt = s_datetime;
  valid = s_datetime_valid;
  rtc_update_pending = s_rtc_update_pending;
  taskEXIT_CRITICAL(&s_datetime_lock);

  if(rtc_update_pending){
    if(!valid){
      return false;
    }

    if(!rtc_driver_set_datetime(&dt)){
      return false;
    }

    taskENTER_CRITICAL(&s_datetime_lock);
    s_rtc_update_pending = false;
    taskEXIT_CRITICAL(&s_datetime_lock);

    return true;
  } else {
    if(!rtc_driver_get_datetime(&dt)){
      return false;
    }

    taskENTER_CRITICAL(&s_datetime_lock);
    s_datetime = dt;
    s_datetime_valid = true;
    taskEXIT_CRITICAL(&s_datetime_lock);

    return true;
  }
}
