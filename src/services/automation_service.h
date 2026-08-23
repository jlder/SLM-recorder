// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/automation_service.h
 * @brief Continuous automatic-start, flight-presence, and flight-end detectors.
 *
 * The service is fed from the State task at 20 Hz. Signal filters and rolling
 * RMS/filter history remains continuous across READY/STARTING/RECORDING
 * transitions. Per-record flight evidence and flight-end sequencing are reset
 * when a recording starts.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "src/drivers/accel_driver.h"

typedef struct {
  float motion_rms_g;
  bool motion_start_confirmed;
  bool attitude_start_confirmed;
  bool motion_stop_confirmed;
  float hirms_g;
  float lowrms_g;
  bool flight_seen;
  bool flight_end_confirmed;
} automation_status_t;

// Read-only diagnostic snapshot. These fields expose detector internals for the
// optional SD diagnostic overlay only; they do not participate in recorder
// state transitions.
typedef struct {
  bool hirms_above;
  bool primary_confirming;
  bool primary_confirmed;
  bool possible_flight;
  bool hirms_event_active;
  bool hirms_event_duration_ok;
  bool hirms_event_saw_flight_fg;
  bool valid_landing_event;
  bool fg_low_confirming;
  bool ground_candidate;
  float flight_ground_norm;
  uint32_t hirms_event_count_samples;
  uint32_t landing_event_age_samples;
  uint32_t fg_low_count_samples;
  uint32_t ground_count_samples;
} automation_debug_status_t;

/** Initialize all automation signal history and logical state. */
void automation_service_init(void);

/**
 * Feed one corrected, installation-aligned acceleration sample.
 *
 * @param sample            20 Hz acceleration sample in milli-g.
 * @param recording_active  true only while the recorder is in ST_RECORDING.
 */
void automation_service_update(const accel_sample_t *sample, bool recording_active);

/**
 * Start a new logical recording session without resetting any filter/RMS history.
 * This distinction is required so a recording that starts while already airborne
 * does not receive an artificial filter-startup transient.
 */
void automation_service_begin_recording(void);

/** Reset only the automatic START confirmation counter. */
void automation_service_reset_start_confirmation(void);

/** Reset all signal history after a deliberate acquisition discontinuity. */
void automation_service_reset_signal_history(void);

/** Return the latest automation detector state. */
automation_status_t automation_service_status(void);

/** Return a read-only snapshot of internal detector diagnostics. */
automation_debug_status_t automation_service_debug_status(void);
