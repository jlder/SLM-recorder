// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/timebase.h
 * @brief Public recording timebase API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "src/drivers/rtc_driver.h"

/**
 * @brief Timebase init.
 *
 * Inputs: None.
 * Returns: None.
 */
void timebase_init(void);
/**
 * @brief Timebase mark record start.
 *
 * Inputs: `dt`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool timebase_mark_record_start(const rtc_datetime_t *dt);
/**
 * @brief Timebase get ms since midnight.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint32_t timebase_get_ms_since_midnight(void);
const char *timebase_get_datetime_compact(void);
