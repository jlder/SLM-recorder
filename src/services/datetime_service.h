// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/datetime_service.h
 * @brief Shared application date/time cache synchronized with RTC hardware.
 *
 * @details
 * The service owns the application-visible date/time value. UI/support code
 * updates the cache when settings are saved. The State task synchronizes the
 * cache with RTC hardware during normal housekeeping.
 */

#pragma once

#include <stdbool.h>
#include "src/drivers/rtc_driver.h"

/**
 * Initialize the date/time cache state.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
void datetime_service_init(void);

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
bool datetime_service_get(rtc_datetime_t *out);

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
bool datetime_service_set(const rtc_datetime_t *in);

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
bool datetime_service_sync_rtc(void);
