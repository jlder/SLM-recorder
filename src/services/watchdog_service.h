// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/watchdog_service.h
 * @brief Software watchdog heartbeat and persistent watchdog-fault API.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  WD_STATE = 0,
  WD_SD,
  WD_RECORD,
  WD_COUNT
} watchdog_source_t;

/**
 * Initializes software watchdog runtime state and loads persistent fault access.
 *
 * Inputs: None.
 * Returns: None.
 */
void watchdog_service_init(void);

/**
 * Records progress for one monitored source.
 *
 * Inputs: `source`.
 * Returns: None.
 */
void watchdog_kick(watchdog_source_t source);

/**
 * Enables or disables timeout enforcement for a monitored source.
 *
 * Inputs: `source`, `required`.
 * Returns: None.
 */
void watchdog_set_required(watchdog_source_t source, bool required);

/**
 * Checks watchdog heartbeats and performs persistent fault/shutdown handling
 * when a required source times out.
 *
 * Inputs: None.
 * Returns: None.
 */
void watchdog_service_check(void);

/**
 * Reports whether a persistent watchdog fault was stored before the previous
 * shutdown.
 *
 * Inputs: None.
 * Returns: `true` when the persistent watchdog fault flag is present.
 */
bool watchdog_persistent_fault_present(void);

/**
 * Clears the persistent watchdog fault flag.
 *
 * Inputs: None.
 * Returns: None.
 */
void watchdog_persistent_fault_clear(void);

#ifdef __cplusplus
}
#endif
