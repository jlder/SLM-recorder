// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_service.h
 * @brief Calibration status and active calibration service.
 */

#pragma once

#include <stdbool.h>
#include "src/models/calibration_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize calibration service and load stored calibration.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if calibration storage initialized, false otherwise.
 */
bool calibration_service_init(void);

/**
 * Refresh calibration status using the latest RTC/date-time cache.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
void calibration_service_refresh_status(void);

/**
 * Return current calibration status.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   calibration status.
 */
calibration_status_t calibration_service_status(void);

/**
 * Return whether recording is allowed from the calibration point of view.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true when latest calibration is valid, not expired, and no calibration fault is latched.
 */
bool calibration_service_is_recording_allowed(void);

/**
 * Copy the active calibration record.
 *
 * Parameters:
 *   out - destination record.
 *
 * Return:
 *   true if a calibration record is available, false otherwise.
 */
bool calibration_service_get_active(calibration_record_t *out);

/**
 * Latch a calibration fault.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
void calibration_service_latch_fault(void);

/**
 * Clear stored calibration data and reset active calibration status.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if calibration storage was cleared successfully, false otherwise.
 */
bool calibration_service_clear(void);

typedef struct {
  bool session_active;
  bool stable;
  bool candidate_valid;
  calibration_face_t candidate_face;
  calibration_vec_t mean_mg;
  calibration_vec_t stddev_mg;
  uint32_t sample_count;

  bool face_valid[CAL_FACE_COUNT];
  calibration_face_capture_t face[CAL_FACE_COUNT];

  bool stored_loaded;
  calibration_face_capture_t stored_face[CAL_FACE_COUNT];
} calibration_sample_status_t;

/**
 * Start a RAM-only calibration session.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if the session was started, false otherwise.
 */
bool calibration_session_start(void);

/**
 * Cancel the active RAM-only calibration session.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
void calibration_session_cancel(void);

/**
 * Return whether a calibration session is active.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if active, false otherwise.
 */
bool calibration_session_active(void);

/**
 * Service the active calibration session.
 *
 * Parameters:
 *   now_ms - current time in milliseconds.
 *
 * Return:
 *   none.
 */
void calibration_session_service(uint32_t now_ms);

/**
 * Copy current calibration sample/stability status.
 *
 * Parameters:
 *   out - destination sample status.
 *
 * Return:
 *   true if status was copied, false otherwise.
 */
bool calibration_session_get_status(calibration_sample_status_t *out);

/**
 * Accept the current stable face candidate into the active calibration session.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if a candidate was accepted, false otherwise.
 */
bool calibration_session_accept_candidate(void);

/**
 * Return whether all six faces have been accepted.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if gains/offsets can be computed, false otherwise.
 */
bool calibration_session_can_compute(void);

/**
 * Compute calibration gains/offsets from accepted faces.
 *
 * Parameters:
 *   out - destination record.
 *
 * Return:
 *   true if computation succeeded and limits were acceptable, false otherwise.
 */
bool calibration_session_compute(calibration_record_t *out);

/**
 * Compute and save calibration as the latest valid calibration.
 *
 * Parameters:
 *   out_saved - optional destination for the saved record; may be null.
 *
 * Return:
 *   true if calibration was saved, false otherwise.
 */
bool calibration_session_save(calibration_record_t *out_saved);

#ifdef __cplusplus
}
#endif
