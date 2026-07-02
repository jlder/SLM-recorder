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
calibration_fault_reason_t calibration_service_fault_reason(void);
const char *calibration_save_result_name(calibration_save_result_t result);

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
bool calibration_service_get_reference(calibration_record_t *out);
bool calibration_service_get_candidate(calibration_record_t *out);
bool calibration_service_get_installation(installation_calibration_t *out);

/**
 * Latch a calibration fault.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
void calibration_service_latch_fault_reason(calibration_fault_reason_t reason);

/**
 * Cancel active calibration sessions without erasing recorder calibration history.
 * Generic field reset uses this path.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if sessions were cancelled.
 */
bool calibration_service_clear(void);

/**
 * Support-only clear of recorder calibration NVS/history.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if calibration storage was cleared successfully, false otherwise.
 */
bool calibration_service_support_clear(void);

/**
 * Support-only clear of installation calibration. Recorder calibration history
 * is preserved.
 */
bool calibration_service_support_clear_installation(void);

typedef struct {
  bool session_active;
  bool stable;
  bool candidate_valid;
  calibration_face_t candidate_face;
  bool current_face_valid;
  calibration_face_t current_face;
  calibration_vec_t mean_mg;
  calibration_vec_t stddev_mg;
  uint32_t sample_count;
  uint32_t current_face_samples;
  uint32_t total_samples;
  uint32_t valid_windows;
  uint32_t total_updates;
  uint32_t last_update_age_ms;
  uint32_t last_update_sample;
  uint32_t face_updates[CAL_FACE_COUNT];
  uint32_t face_last_update_age_ms[CAL_FACE_COUNT];
  uint32_t face_last_update_sample[CAL_FACE_COUNT];
  float face_quality_mg[CAL_FACE_COUNT];

  bool face_valid[CAL_FACE_COUNT];
  calibration_face_capture_t face[CAL_FACE_COUNT];

  bool stored_loaded;
  calibration_face_capture_t stored_face[CAL_FACE_COUNT];

  bool temperature_available;
  bool temperature_in_range;
  bool temperature_stable;
  float temperature_c;
  float temperature_min_c;
  float temperature_max_c;
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
bool calibration_session_save_with_result(calibration_record_t *out_saved, calibration_save_result_t *out_result);

typedef struct {
  bool session_active;
  bool stable;
  bool candidate_valid;
  calibration_vec_t mean_mg;
  calibration_vec_t stddev_mg;
  uint32_t sample_count;
  uint32_t total_samples;
  uint32_t valid_windows;
  uint32_t update_count;
  uint32_t last_update_age_ms;
  float quality_mg;
  float matrix[9];

  bool stored_valid;
  rtc_datetime_t stored_timestamp;
  calibration_vec_t stored_mean_mg;
  calibration_vec_t stored_stddev_mg;
  float stored_matrix[9];
} installation_calibration_status_t;

/** Start a RAM-only installation calibration session. */
bool calibration_installation_session_start(void);

/** Cancel the active installation calibration session. */
void calibration_installation_session_cancel(void);

/** Return whether installation calibration session is active. */
bool calibration_installation_session_active(void);

/** Copy current installation calibration sample/status. */
bool calibration_installation_session_get_status(installation_calibration_status_t *out);

/** Save the most recent stable installation calibration candidate. */
bool calibration_installation_session_save(calibration_record_t *out_saved);

/** Return whether the active calibration record includes installation correction. */
bool calibration_service_installation_valid(void);

#ifdef __cplusplus
}
#endif
