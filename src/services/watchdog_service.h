// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/watchdog_service.h
 * @brief Software watchdog heartbeat and persistent watchdog-fault API.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  WD_STATE = 0,
  WD_SD,
  WD_RECORD,
  WD_WEB,
  WD_COUNT
} watchdog_source_t;

/**
 * Snapshot of the last watchdog fault stored in NVS.
 *
 * `active` tells whether the fault is still waiting for operator
 * acknowledgement.  The diagnostic values remain available after the active
 * fault is cleared so the Web maintenance page can still show the cause.
 */
typedef struct {
  bool active;
  watchdog_source_t source;
  uint32_t age_ms;
  uint32_t ages_ms[WD_COUNT];
  uint32_t recorder_state;
  int32_t last_error;
  bool web_active;
  bool usb_present;
  bool sd_present;
  uint32_t heap;
  uint32_t min_heap;
} watchdog_fault_info_t;

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
 * Clears the active persistent watchdog fault flag.  The last diagnostic
 * snapshot is retained for the Web maintenance page.
 *
 * Inputs: None.
 * Returns: None.
 */
void watchdog_persistent_fault_clear(void);

/**
 * Reads the last stored watchdog fault diagnostic.
 *
 * Inputs: `info` output pointer.
 * Returns: `true` when diagnostic data is available.
 */
bool watchdog_get_fault_info(watchdog_fault_info_t *info);

/**
 * Returns a short printable name for a watchdog source.
 *
 * Inputs: `source`.
 * Returns: Constant source name string.
 */
const char *watchdog_source_name(watchdog_source_t source);

#ifdef __cplusplus
}
#endif
