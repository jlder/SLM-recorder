// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_service.cpp
 * @brief Calibration status and active calibration service.
 */

#include "src/services/calibration_service.h"

#include <string.h>
#include <math.h>
#include <Arduino.h>
#include "config.h"
#include "src/services/calibration_store.h"
#include "src/services/datetime_service.h"
#include "src/drivers/accel_driver.h"

static calibration_record_t s_active_cal = {};
static bool s_active_loaded = false;
static calibration_status_t s_status = CAL_STATUS_MISSING;

static bool s_session_active = false;
static calibration_face_capture_t s_session_face[CAL_FACE_COUNT] = {};

static uint32_t s_last_sample_ms = 0u;

static calibration_vec_t s_window[CALIBRATION_WINDOW_SAMPLE_COUNT] = {};
static uint32_t s_window_index = 0u;
static uint32_t s_window_count = 0u;

static bool s_latest_stable = false;
static calibration_vec_t s_latest_mean = {};
static calibration_vec_t s_latest_stddev = {};

static bool s_candidate_valid = false;
static calibration_face_t s_candidate_face = CAL_FACE_PX;
static calibration_vec_t s_candidate_mean = {};
static calibration_vec_t s_candidate_stddev = {};

static bool s_install_session_active = false;
static bool s_install_candidate_valid = false;
static calibration_vec_t s_install_candidate_mean = {};
static calibration_vec_t s_install_candidate_stddev = {};
static float s_install_candidate_matrix[9] = {1.0f, 0.0f, 0.0f,
                                             0.0f, 1.0f, 0.0f,
                                             0.0f, 0.0f, 1.0f};
static float s_install_candidate_quality = 999999.0f;

static bool calibration_detect_face_(const calibration_vec_t *mean, calibration_face_t *out_face);
static void calibration_store_session_face_(calibration_face_t face,
                                            const calibration_vec_t *mean,
                                            const calibration_vec_t *stddev);

/**
 * Convert a calibration record into driver calibration parameters.
 *
 * Parameters:
 *   rec - calibration record.
 *   out - destination driver calibration.
 *
 * Return:
 *   true if conversion succeeded, false otherwise.
 */
static bool calibration_to_driver_(const calibration_record_t *rec, accel_calibration_t *out){
  if((rec == nullptr) || (out == nullptr)){
    return false;
  }

  out->gain_x = rec->sensor.gain_x;
  out->gain_y = rec->sensor.gain_y;
  out->gain_z = rec->sensor.gain_z;
  out->offset_x_mg = rec->sensor.offset_x_mg;
  out->offset_y_mg = rec->sensor.offset_y_mg;
  out->offset_z_mg = rec->sensor.offset_z_mg;
  return true;
}

/**
 * Return whether a timestamp is older than the configured validity in months.
 *
 * Parameters:
 *   cal - calibration timestamp.
 *   now - current date/time.
 *
 * Return:
 *   true if calibration is older than allowed, false otherwise.
 */
static bool calibration_is_expired_(const rtc_datetime_t *cal, const rtc_datetime_t *now){
  if((cal == nullptr) || (now == nullptr)){
    return true;
  }

  const int32_t cal_month = ((int32_t)cal->year * 12) + (int32_t)cal->month;
  const int32_t now_month = ((int32_t)now->year * 12) + (int32_t)now->month;
  const int32_t delta_months = now_month - cal_month;

  if(delta_months < 0){
    return false;
  }

  if(delta_months > (int32_t)CALIBRATION_VALIDITY_MONTHS){
    return true;
  }

  if(delta_months == (int32_t)CALIBRATION_VALIDITY_MONTHS){
    return (now->day > cal->day);
  }

  return false;
}

/**
 * Apply or clear driver calibration according to current service status.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
static void calibration_apply_driver_state_(void){
  if(s_status != CAL_STATUS_VALID){
    accel_driver_clear_calibration();
    accel_driver_clear_installation();
    return;
  }

  accel_calibration_t driver_cal = {};
  if(calibration_to_driver_(&s_active_cal, &driver_cal)){
    (void)accel_driver_set_calibration(&driver_cal);
  } else {
    accel_driver_clear_calibration();
  }

  if(s_active_cal.installation.valid){
    accel_installation_t installation = {};
    memcpy(installation.matrix, s_active_cal.installation.matrix, sizeof(installation.matrix));
    (void)accel_driver_set_installation(&installation);
  } else {
    accel_driver_clear_installation();
  }
}

/**
 * Initializes calibration storage, loads the latest stored calibration,
 * applies it to the driver, and initializes calibration status.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_service_init(void){
  const bool storage_ok = calibration_store_init();

  s_active_loaded = false;
  s_status = CAL_STATUS_MISSING;
  memset(&s_active_cal, 0, sizeof(s_active_cal));

  if(storage_ok && calibration_store_load(&s_active_cal)){
    s_active_loaded = true;
  }

  calibration_service_refresh_status();
  return storage_ok;
}

/**
 * Refreshes the effective calibration status using stored calibration,
 * calibration age, and calibration fault latch state.
 *
 * Inputs: None.
 * Returns: None.
 */
void calibration_service_refresh_status(void){
  if(calibration_store_fault_get()){
    s_status = CAL_STATUS_FAULT;
    calibration_apply_driver_state_();
    return;
  }

  if(!s_active_loaded || !s_active_cal.sensor.valid){
    s_status = CAL_STATUS_MISSING;
    calibration_apply_driver_state_();
    return;
  }

  rtc_datetime_t now = {};
  if(!datetime_service_get(&now)){
    // Date/time is required to prove calibration freshness.
    s_status = CAL_STATUS_MISSING;
    calibration_apply_driver_state_();
    return;
  }

  if(calibration_is_expired_(&s_active_cal.sensor.timestamp, &now)){
    s_status = CAL_STATUS_EXPIRED;
    calibration_apply_driver_state_();
    return;
  }

  s_status = CAL_STATUS_VALID;
  calibration_apply_driver_state_();
}

/**
 * Returns the current calibration status used by the state machine to
 * authorize or block recording.
 *
 * Inputs: None.
 * Returns: Requested value.
 */
calibration_status_t calibration_service_status(void){
  return s_status;
}

/**
 * Reports whether calibration status allows recording to start.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_service_is_recording_allowed(void){
  calibration_service_refresh_status();
  return (s_status == CAL_STATUS_VALID) && s_active_cal.installation.valid;
}

bool calibration_service_installation_valid(void){
  return (s_status == CAL_STATUS_VALID) && s_active_cal.installation.valid;
}

/**
 * Copies the active calibration record for use by recording-file formatting
 * and Web/API display.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_service_get_active(calibration_record_t *out){
  if((out == nullptr) || !s_active_loaded){
    return false;
  }
  *out = s_active_cal;
  return true;
}

/**
 * Latches calibration fault state in NVS, marks calibration status as faulted,
 * and disables driver calibration use.
 *
 * Inputs: None.
 * Returns: None.
 */
void calibration_service_latch_fault(void){
  (void)calibration_store_fault_set(true);
  s_status = CAL_STATUS_FAULT;
  calibration_apply_driver_state_();
}

/**
 * Clears stored calibration data, resets active calibration state, cancels any
 * session, and removes driver correction.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_service_clear(void){
  const bool ok = calibration_store_clear();

  s_active_loaded = false;
  memset(&s_active_cal, 0, sizeof(s_active_cal));
  s_status = CAL_STATUS_MISSING;
  calibration_apply_driver_state_();

  calibration_session_cancel();
  calibration_installation_session_cancel();
  return ok;
}

/**
 * Reset the current sample stability window.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
static void calibration_window_reset_(void){
  memset(s_window, 0, sizeof(s_window));
  s_window_index = 0u;
  s_window_count = 0u;
  s_latest_stable = false;
  s_latest_mean = {};
  s_latest_stddev = {};
}

/**
 * Clear the currently proposed calibration candidate.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
static void calibration_candidate_clear_(void){
  s_candidate_valid = false;
  s_candidate_face = CAL_FACE_PX;
  s_candidate_mean = {};
  s_candidate_stddev = {};
}

/**
 * Return the dominant absolute acceleration component.
 *
 * Parameters:
 *   v - acceleration vector.
 *
 * Return:
 *   dominant absolute value in milli-g.
 */
static float calibration_dominant_abs_(const calibration_vec_t *v){
  if(v == nullptr){
    return 0.0f;
  }

  float dominant = fabsf(v->x_mg);
  const float ay = fabsf(v->y_mg);
  const float az = fabsf(v->z_mg);

  if(ay > dominant){
    dominant = ay;
  }
  if(az > dominant){
    dominant = az;
  }

  return dominant;
}

/**
 * Return whether a raw sample is within the calibration face acceptance range.
 *
 * Parameters:
 *   v - raw acceleration vector.
 *
 * Return:
 *   true if the dominant axis is within the configured gravity tolerance.
 */
static bool calibration_raw_sample_in_range_(const calibration_vec_t *v){
  const float dominant = calibration_dominant_abs_(v);
  const float tol_mg = CALIBRATION_GRAVITY_MG * (CALIBRATION_FACE_GRAVITY_TOL_PCT / 100.0f);
  return (fabsf(dominant - CALIBRATION_GRAVITY_MG) <= tol_mg);
}

/**
 * Push a sample into the circular calibration window.
 *
 * Parameters:
 *   v - raw sample to store.
 *
 * Return:
 *   none.
 */
static void calibration_window_push_(const calibration_vec_t *v){
  if(v == nullptr){
    return;
  }

  s_window[s_window_index] = *v;
  s_window_index = (s_window_index + 1u) % (uint32_t)CALIBRATION_WINDOW_SAMPLE_COUNT;

  if(s_window_count < (uint32_t)CALIBRATION_WINDOW_SAMPLE_COUNT){
    s_window_count++;
  }
}

/**
 * Evaluate the full circular window for stability and face candidate.
 *
 * Parameters:
 *   none.
 *
 * Return:
 *   none.
 */
static void calibration_window_evaluate_(void){
  if(s_window_count < (uint32_t)CALIBRATION_WINDOW_SAMPLE_COUNT){
    s_latest_stable = false;
    return;
  }

  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_z = 0.0;
  double sum2_x = 0.0;
  double sum2_y = 0.0;
  double sum2_z = 0.0;

  for(uint32_t i = 0u; i < (uint32_t)CALIBRATION_WINDOW_SAMPLE_COUNT; ++i){
    sum_x += (double)s_window[i].x_mg;
    sum_y += (double)s_window[i].y_mg;
    sum_z += (double)s_window[i].z_mg;

    sum2_x += (double)s_window[i].x_mg * (double)s_window[i].x_mg;
    sum2_y += (double)s_window[i].y_mg * (double)s_window[i].y_mg;
    sum2_z += (double)s_window[i].z_mg * (double)s_window[i].z_mg;
  }

  const double n = (double)CALIBRATION_WINDOW_SAMPLE_COUNT;

  calibration_vec_t mean = {};
  calibration_vec_t stddev = {};

  mean.x_mg = (float)(sum_x / n);
  mean.y_mg = (float)(sum_y / n);
  mean.z_mg = (float)(sum_z / n);

  const double vx = (sum2_x / n) - ((double)mean.x_mg * (double)mean.x_mg);
  const double vy = (sum2_y / n) - ((double)mean.y_mg * (double)mean.y_mg);
  const double vz = (sum2_z / n) - ((double)mean.z_mg * (double)mean.z_mg);

  stddev.x_mg = (float)sqrt((vx > 0.0) ? vx : 0.0);
  stddev.y_mg = (float)sqrt((vy > 0.0) ? vy : 0.0);
  stddev.z_mg = (float)sqrt((vz > 0.0) ? vz : 0.0);

  s_latest_mean = mean;
  s_latest_stddev = stddev;

  s_latest_stable =
      (stddev.x_mg <= CALIBRATION_STABILITY_STDDEV_MAX_MG) &&
      (stddev.y_mg <= CALIBRATION_STABILITY_STDDEV_MAX_MG) &&
      (stddev.z_mg <= CALIBRATION_STABILITY_STDDEV_MAX_MG);

  if(!s_latest_stable){
    // The full window was evaluated and found unstable. Reset it so motion
    // samples do not continue to pollute the next stability window.
    calibration_window_reset_();
    return;
  }

  calibration_face_t detected_face = CAL_FACE_PX;
  if(calibration_detect_face_(&mean, &detected_face)){
    s_candidate_valid = true;
    s_candidate_face = detected_face;
    s_candidate_mean = mean;
    s_candidate_stddev = stddev;

    // A valid stable face is offered to the current calibration session.
    // If the same face already has a current-session value, only a lower
    // standard-deviation result replaces it. NVS/stored calibration is not used
    // for this decision.
    calibration_store_session_face_(detected_face, &mean, &stddev);

    // Once a valid capture has been stored, reset the rolling window so the
    // same samples cannot be reused for the next evaluation.
    calibration_window_reset_();
  }
}

/**
 * Copy current session face-valid flags to status output.
 *
 * Parameters:
 *   out - destination status.
 *
 * Return:
 *   none.
 */
static void calibration_copy_face_status_(calibration_sample_status_t *out){
  if(out == nullptr){
    return;
  }

  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    out->face_valid[i] = s_session_face[i].valid;
    out->face[i] = s_session_face[i];

    if(s_active_loaded){
      out->stored_face[i] = s_active_cal.sensor.face[i];
    }
  }

  out->stored_loaded = s_active_loaded;
}

/**
 * Infer calibration face from a stable mean vector.
 *
 * Parameters:
 *   mean - stable mean vector.
 *   out_face - detected face.
 *
 * Return:
 *   true if a face was detected and dominant axis is within tolerance.
 */
static bool calibration_detect_face_(const calibration_vec_t *mean, calibration_face_t *out_face){
  if((mean == nullptr) || (out_face == nullptr)){
    return false;
  }

  const float ax = fabsf(mean->x_mg);
  const float ay = fabsf(mean->y_mg);
  const float az = fabsf(mean->z_mg);

  float dominant = ax;
  calibration_face_t face = (mean->x_mg >= 0.0f) ? CAL_FACE_PX : CAL_FACE_NX;

  if(ay > dominant){
    dominant = ay;
    face = (mean->y_mg >= 0.0f) ? CAL_FACE_PY : CAL_FACE_NY;
  }

  if(az > dominant){
    dominant = az;
    face = (mean->z_mg >= 0.0f) ? CAL_FACE_PZ : CAL_FACE_NZ;
  }

  const float tol_mg = CALIBRATION_GRAVITY_MG * (CALIBRATION_FACE_GRAVITY_TOL_PCT / 100.0f);
  if(fabsf(dominant - CALIBRATION_GRAVITY_MG) > tol_mg){
    return false;
  }

  *out_face = face;
  return true;
}

/**
 * Return whether all accepted faces are present.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if all six faces were accepted.
 */
static bool calibration_all_faces_valid_(void){
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    if(!s_session_face[i].valid){
      return false;
    }
  }
  return true;
}

/**
 * Return target component for a calibration face.
 *
 * Parameters:
 *   face - face identifier.
 *
 * Return:
 *   dominant-axis raw value in milli-g for the face.
 */
static float calibration_face_axis_value_(calibration_face_t face){
  switch(face){
    case CAL_FACE_PX: return s_session_face[CAL_FACE_PX].mean_mg.x_mg;
    case CAL_FACE_NX: return s_session_face[CAL_FACE_NX].mean_mg.x_mg;
    case CAL_FACE_PY: return s_session_face[CAL_FACE_PY].mean_mg.y_mg;
    case CAL_FACE_NY: return s_session_face[CAL_FACE_NY].mean_mg.y_mg;
    case CAL_FACE_PZ: return s_session_face[CAL_FACE_PZ].mean_mg.z_mg;
    case CAL_FACE_NZ: return s_session_face[CAL_FACE_NZ].mean_mg.z_mg;
    default: return 0.0f;
  }
}

/**
 * Return whether a computed gain/offset pair is plausible.
 *
 * Parameters:
 *   gain - computed gain.
 *   offset_mg - computed offset.
 *
 * Return:
 *   true if within configured plausibility limits.
 */
static bool calibration_gain_offset_plausible_(float gain, float offset_mg){
  if((gain < CALIBRATION_GAIN_MIN) || (gain > CALIBRATION_GAIN_MAX)){
    return false;
  }

  if(fabsf(offset_mg) > CALIBRATION_OFFSET_ABS_MAX_MG){
    return false;
  }

  return true;
}

/**
 * Return one scalar quality metric from a 3-axis standard-deviation vector.
 *
 * Parameters:
 *   stddev - standard-deviation vector.
 *
 * Return:
 *   maximum axis standard deviation in milli-g.
 */
static float calibration_stddev_quality_(const calibration_vec_t *stddev){
  if(stddev == nullptr){
    return 999999.0f;
  }

  float q = stddev->x_mg;
  if(stddev->y_mg > q){
    q = stddev->y_mg;
  }
  if(stddev->z_mg > q){
    q = stddev->z_mg;
  }

  return q;
}

/**
 * Store the best current-session capture for one face.
 *
 * Parameters:
 *   face - detected face.
 *   mean - rolling-window mean.
 *   stddev - rolling-window standard deviation.
 *
 * Return:
 *   none.
 */
static void calibration_store_session_face_(calibration_face_t face,
                                            const calibration_vec_t *mean,
                                            const calibration_vec_t *stddev){
  if((mean == nullptr) || (stddev == nullptr)){
    return;
  }

  calibration_face_capture_t *dst = &s_session_face[(uint32_t)face];

  if(dst->valid){
    const float current_quality = calibration_stddev_quality_(&dst->stddev_mg);
    const float new_quality = calibration_stddev_quality_(stddev);

    // During a calibration session, keep the best stable capture for each face.
    // Stored/NVS calibration is not part of this decision; it is displayed only
    // for comparison and traceability.
    if(new_quality >= current_quality){
      return;
    }
  }

  dst->valid = true;
  dst->mean_mg = *mean;
  dst->stddev_mg = *stddev;
}

/**
 * Starts a new RAM-only calibration session, clearing previous session
 * captures and resetting rolling-window state.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_session_start(void){
  calibration_installation_session_cancel();
  s_session_active = true;
  memset(s_session_face, 0, sizeof(s_session_face));
  calibration_candidate_clear_();
  calibration_window_reset_();
  s_last_sample_ms = 0u;
  return true;
}

/**
 * Cancels the active calibration session and clears all current-session face
 * captures and candidate data.
 *
 * Inputs: None.
 * Returns: None.
 */
void calibration_session_cancel(void){
  s_session_active = false;
  memset(s_session_face, 0, sizeof(s_session_face));
  calibration_candidate_clear_();
  calibration_window_reset_();
  s_last_sample_ms = 0u;
}

/**
 * Reports whether a calibration session is currently active.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_session_active(void){
  return s_session_active;
}


static void calibration_matrix_identity_(float m[9]){
  if(m == nullptr){
    return;
  }
  m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f;
  m[3] = 0.0f; m[4] = 1.0f; m[5] = 0.0f;
  m[6] = 0.0f; m[7] = 0.0f; m[8] = 1.0f;
}

static float calibration_vec_norm_(const calibration_vec_t *v){
  if(v == nullptr){
    return 0.0f;
  }
  return sqrtf((v->x_mg * v->x_mg) + (v->y_mg * v->y_mg) + (v->z_mg * v->z_mg));
}

static bool calibration_window_stats_(calibration_vec_t *mean, calibration_vec_t *stddev){
  if((mean == nullptr) || (stddev == nullptr)){
    return false;
  }
  if(s_window_count < (uint32_t)CALIBRATION_WINDOW_SAMPLE_COUNT){
    return false;
  }

  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_z = 0.0;
  double sum2_x = 0.0;
  double sum2_y = 0.0;
  double sum2_z = 0.0;

  for(uint32_t i = 0u; i < (uint32_t)CALIBRATION_WINDOW_SAMPLE_COUNT; ++i){
    sum_x += (double)s_window[i].x_mg;
    sum_y += (double)s_window[i].y_mg;
    sum_z += (double)s_window[i].z_mg;

    sum2_x += (double)s_window[i].x_mg * (double)s_window[i].x_mg;
    sum2_y += (double)s_window[i].y_mg * (double)s_window[i].y_mg;
    sum2_z += (double)s_window[i].z_mg * (double)s_window[i].z_mg;
  }

  const double n = (double)CALIBRATION_WINDOW_SAMPLE_COUNT;

  mean->x_mg = (float)(sum_x / n);
  mean->y_mg = (float)(sum_y / n);
  mean->z_mg = (float)(sum_z / n);

  const double vx = (sum2_x / n) - ((double)mean->x_mg * (double)mean->x_mg);
  const double vy = (sum2_y / n) - ((double)mean->y_mg * (double)mean->y_mg);
  const double vz = (sum2_z / n) - ((double)mean->z_mg * (double)mean->z_mg);

  stddev->x_mg = (float)sqrt((vx > 0.0) ? vx : 0.0);
  stddev->y_mg = (float)sqrt((vy > 0.0) ? vy : 0.0);
  stddev->z_mg = (float)sqrt((vz > 0.0) ? vz : 0.0);
  return true;
}

static bool calibration_stable_(const calibration_vec_t *stddev){
  if(stddev == nullptr){
    return false;
  }
  return (stddev->x_mg <= CALIBRATION_STABILITY_STDDEV_MAX_MG) &&
         (stddev->y_mg <= CALIBRATION_STABILITY_STDDEV_MAX_MG) &&
         (stddev->z_mg <= CALIBRATION_STABILITY_STDDEV_MAX_MG);
}

static bool installation_matrix_from_gravity_(const calibration_vec_t *mean, float out_matrix[9]){
  if((mean == nullptr) || (out_matrix == nullptr)){
    return false;
  }

  const float norm = calibration_vec_norm_(mean);
  const float tol_mg = CALIBRATION_GRAVITY_MG * (INSTALLATION_GRAVITY_TOL_PCT / 100.0f);
  if((norm < 1.0f) || (fabsf(norm - CALIBRATION_GRAVITY_MG) > tol_mg)){
    return false;
  }

  const float ax = mean->x_mg / norm;
  const float ay = mean->y_mg / norm;
  const float az = mean->z_mg / norm;

  // Rotation from measured gravity vector a to target +Z vector b=(0,0,1).
  // v = a x b = (ay, -ax, 0), c = a dot b = az.
  const float vx = ay;
  const float vy = -ax;
  const float vz = 0.0f;
  const float c = az;
  const float s2 = (vx * vx) + (vy * vy) + (vz * vz);

  if(s2 < 1.0e-8f){
    if(c >= 0.0f){
      calibration_matrix_identity_(out_matrix);
    } else {
      // 180 degree rotation around X is one valid solution for the opposite vector.
      out_matrix[0] = 1.0f;  out_matrix[1] = 0.0f;  out_matrix[2] = 0.0f;
      out_matrix[3] = 0.0f;  out_matrix[4] = -1.0f; out_matrix[5] = 0.0f;
      out_matrix[6] = 0.0f;  out_matrix[7] = 0.0f;  out_matrix[8] = -1.0f;
    }
    return true;
  }

  const float k = (1.0f - c) / s2;

  const float k00 = 0.0f;
  const float k01 = -vz;
  const float k02 = vy;
  const float k10 = vz;
  const float k11 = 0.0f;
  const float k12 = -vx;
  const float k20 = -vy;
  const float k21 = vx;
  const float k22 = 0.0f;

  out_matrix[0] = 1.0f + k00 + k * ((k00 * k00) + (k01 * k10) + (k02 * k20));
  out_matrix[1] =        k01 + k * ((k00 * k01) + (k01 * k11) + (k02 * k21));
  out_matrix[2] =        k02 + k * ((k00 * k02) + (k01 * k12) + (k02 * k22));
  out_matrix[3] =        k10 + k * ((k10 * k00) + (k11 * k10) + (k12 * k20));
  out_matrix[4] = 1.0f + k11 + k * ((k10 * k01) + (k11 * k11) + (k12 * k21));
  out_matrix[5] =        k12 + k * ((k10 * k02) + (k11 * k12) + (k12 * k22));
  out_matrix[6] =        k20 + k * ((k20 * k00) + (k21 * k10) + (k22 * k20));
  out_matrix[7] =        k21 + k * ((k20 * k01) + (k21 * k11) + (k22 * k21));
  out_matrix[8] = 1.0f + k22 + k * ((k20 * k02) + (k21 * k12) + (k22 * k22));
  return true;
}

static void installation_session_service_(uint32_t now_ms){
  if(!s_install_session_active){
    return;
  }

  if((s_last_sample_ms != 0u) &&
     ((now_ms - s_last_sample_ms) < (uint32_t)CALIBRATION_SAMPLE_PERIOD_MS)){
    return;
  }
  s_last_sample_ms = now_ms;

  accel_sample_t sample = {};
  if(!accel_read_xyz_sensor_corrected(&sample)){
    calibration_window_reset_();
    return;
  }

  const calibration_vec_t v = {
    (float)sample.ax,
    (float)sample.ay,
    (float)sample.az
  };

  const float norm = calibration_vec_norm_(&v);
  const float tol_mg = CALIBRATION_GRAVITY_MG * (INSTALLATION_GRAVITY_TOL_PCT / 100.0f);
  if((norm < 1.0f) || (fabsf(norm - CALIBRATION_GRAVITY_MG) > tol_mg)){
    calibration_window_reset_();
    return;
  }

  calibration_window_push_(&v);

  calibration_vec_t mean = {};
  calibration_vec_t stddev = {};
  if(!calibration_window_stats_(&mean, &stddev)){
    return;
  }

  s_latest_mean = mean;
  s_latest_stddev = stddev;
  s_latest_stable = calibration_stable_(&stddev);

  if(!s_latest_stable){
    calibration_window_reset_();
    return;
  }

  float matrix[9] = {};
  if(!installation_matrix_from_gravity_(&mean, matrix)){
    calibration_window_reset_();
    return;
  }

  const float quality = calibration_stddev_quality_(&stddev);
  if((!s_install_candidate_valid) || (quality < s_install_candidate_quality)){
    s_install_candidate_valid = true;
    s_install_candidate_mean = mean;
    s_install_candidate_stddev = stddev;
    memcpy(s_install_candidate_matrix, matrix, sizeof(s_install_candidate_matrix));
    s_install_candidate_quality = quality;
  }

  calibration_window_reset_();
}

/**
 * Calibration session service samples raw acceleration at the configured rate,
 * maintains the rolling stability window, detects faces, and updates the
 * current calibration session.
 *
 * Inputs: `now_ms`.
 * Returns: None.
 */
void calibration_session_service(uint32_t now_ms){
  if(s_install_session_active){
    installation_session_service_(now_ms);
    return;
  }

  if(!s_session_active){
    return;
  }

  if((s_last_sample_ms != 0u) &&
     ((now_ms - s_last_sample_ms) < (uint32_t)CALIBRATION_SAMPLE_PERIOD_MS)){
    return;
  }
  s_last_sample_ms = now_ms;

  accel_sample_t raw = {};
  if(!accel_read_xyz_raw(&raw)){
    calibration_window_reset_();
    return;
  }

  const calibration_vec_t v = {
    (float)raw.ax,
    (float)raw.ay,
    (float)raw.az
  };

  // Samples outside the expected gravity range indicate transition/motion or
  // a non-face orientation. Reset the circular window so those values do not
  // contribute to the stability calculation.
  if(!calibration_raw_sample_in_range_(&v)){
    calibration_window_reset_();
    return;
  }

  calibration_window_push_(&v);
  calibration_window_evaluate_();
}

/**
 * Copies current calibration session status, candidate data, face captures,
 * and stored NVS comparison data for Web display.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_session_get_status(calibration_sample_status_t *out){
  if(out == nullptr){
    return false;
  }

  memset(out, 0, sizeof(*out));
  out->session_active = s_session_active;
  out->stable = s_latest_stable;
  out->candidate_valid = s_candidate_valid;
  out->candidate_face = s_candidate_face;
  out->mean_mg = s_candidate_valid ? s_candidate_mean : s_latest_mean;
  out->stddev_mg = s_candidate_valid ? s_candidate_stddev : s_latest_stddev;
  out->sample_count = s_window_count;
  calibration_copy_face_status_(out);
  return true;
}

/**
 * Legacy manual-accept hook retained for compatibility; current auto-capture
 * workflow does not require per-face manual acceptance.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_session_accept_candidate(void){
  // Manual acceptance is retained for API compatibility. The rolling-window
  // service now stores valid face captures automatically.
  if(!s_session_active || !s_candidate_valid){
    return false;
  }

  calibration_store_session_face_(s_candidate_face, &s_candidate_mean, &s_candidate_stddev);
  calibration_candidate_clear_();
  return true;
}

/**
 * Reports whether all six face captures are available so gain and offset can
 * be calculated.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_session_can_compute(void){
  return s_session_active && calibration_all_faces_valid_();
}

/**
 * Computes gain and offset from the six captured face values and validates
 * calibration plausibility limits.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
static bool calibration_session_compute_internal_(calibration_record_t *out, bool latch_fault_on_error){
  if(out == nullptr){
    return false;
  }

  if(!calibration_session_can_compute()){
    return false;
  }

  rtc_datetime_t now = {};
  if(!datetime_service_get(&now)){
    return false;
  }

  const float x_pos = calibration_face_axis_value_(CAL_FACE_PX);
  const float x_neg = calibration_face_axis_value_(CAL_FACE_NX);
  const float y_pos = calibration_face_axis_value_(CAL_FACE_PY);
  const float y_neg = calibration_face_axis_value_(CAL_FACE_NY);
  const float z_pos = calibration_face_axis_value_(CAL_FACE_PZ);
  const float z_neg = calibration_face_axis_value_(CAL_FACE_NZ);

  if((fabsf(x_pos - x_neg) < 1.0f) ||
     (fabsf(y_pos - y_neg) < 1.0f) ||
     (fabsf(z_pos - z_neg) < 1.0f)){
    if(latch_fault_on_error){
      calibration_service_latch_fault();
    }
    return false;
  }

  calibration_record_t rec = {};
  rec.version = (uint32_t)CALIBRATION_RECORD_VERSION;
  rec.sensor.valid = true;
  rec.sensor.timestamp = now;
  rec.sensor.gravity_mg = CALIBRATION_GRAVITY_MG;

  rec.sensor.gain_x = (2.0f * CALIBRATION_GRAVITY_MG) / (x_pos - x_neg);
  rec.sensor.gain_y = (2.0f * CALIBRATION_GRAVITY_MG) / (y_pos - y_neg);
  rec.sensor.gain_z = (2.0f * CALIBRATION_GRAVITY_MG) / (z_pos - z_neg);

  rec.sensor.offset_x_mg = -rec.sensor.gain_x * ((x_pos + x_neg) * 0.5f);
  rec.sensor.offset_y_mg = -rec.sensor.gain_y * ((y_pos + y_neg) * 0.5f);
  rec.sensor.offset_z_mg = -rec.sensor.gain_z * ((z_pos + z_neg) * 0.5f);

  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    rec.sensor.face[i] = s_session_face[i];
  }

  if(!calibration_gain_offset_plausible_(rec.sensor.gain_x, rec.sensor.offset_x_mg) ||
     !calibration_gain_offset_plausible_(rec.sensor.gain_y, rec.sensor.offset_y_mg) ||
     !calibration_gain_offset_plausible_(rec.sensor.gain_z, rec.sensor.offset_z_mg)){
    if(latch_fault_on_error){
      calibration_service_latch_fault();
    }
    return false;
  }

  *out = rec;
  return true;
}


/**
 * Computes gain and offset from the six captured face values without making
 * persistent changes. Preview/result endpoints use this side-effect-free path.
 *
 * Inputs: `out`.
 * Returns: `true` when computation succeeded and limits were acceptable.
 */
bool calibration_session_compute(calibration_record_t *out){
  return calibration_session_compute_internal_(out, false);
}

/**
 * Computes and stores the new calibration, clears any calibration fault latch
 * on success, and applies the new coefficients to the driver.
 *
 * Inputs: `out_saved`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool calibration_session_save(calibration_record_t *out_saved){
  calibration_record_t rec = {};
  if(!calibration_session_compute_internal_(&rec, true)){
    return false;
  }

  if(!calibration_store_save_latest(&rec)){
    return false;
  }

  s_active_cal = rec;
  s_active_loaded = true;
  s_session_active = false;
  calibration_service_refresh_status();

  if(out_saved != nullptr){
    *out_saved = rec;
  }

  return true;
}


bool calibration_installation_session_start(void){
  calibration_service_refresh_status();
  if(s_status != CAL_STATUS_VALID){
    return false;
  }

  calibration_session_cancel();
  s_install_session_active = true;
  s_install_candidate_valid = false;
  s_install_candidate_mean = {};
  s_install_candidate_stddev = {};
  calibration_matrix_identity_(s_install_candidate_matrix);
  s_install_candidate_quality = 999999.0f;
  calibration_window_reset_();
  s_last_sample_ms = 0u;
  return true;
}

void calibration_installation_session_cancel(void){
  s_install_session_active = false;
  s_install_candidate_valid = false;
  s_install_candidate_mean = {};
  s_install_candidate_stddev = {};
  calibration_matrix_identity_(s_install_candidate_matrix);
  s_install_candidate_quality = 999999.0f;
  calibration_window_reset_();
  s_last_sample_ms = 0u;
}

bool calibration_installation_session_active(void){
  return s_install_session_active;
}

bool calibration_installation_session_get_status(installation_calibration_status_t *out){
  if(out == nullptr){
    return false;
  }

  memset(out, 0, sizeof(*out));
  out->session_active = s_install_session_active;
  out->stable = s_latest_stable;
  out->candidate_valid = s_install_candidate_valid;
  out->mean_mg = s_install_candidate_valid ? s_install_candidate_mean : s_latest_mean;
  out->stddev_mg = s_install_candidate_valid ? s_install_candidate_stddev : s_latest_stddev;
  out->sample_count = s_window_count;
  memcpy(out->matrix, s_install_candidate_matrix, sizeof(out->matrix));

  if(s_active_loaded && s_active_cal.installation.valid){
    out->stored_valid = true;
    out->stored_timestamp = s_active_cal.installation.timestamp;
    out->stored_mean_mg = s_active_cal.installation.mean_mg;
    out->stored_stddev_mg = s_active_cal.installation.stddev_mg;
    memcpy(out->stored_matrix, s_active_cal.installation.matrix, sizeof(out->stored_matrix));
  }

  return true;
}

bool calibration_installation_session_save(calibration_record_t *out_saved){
  if(!s_install_session_active || !s_install_candidate_valid){
    return false;
  }

  calibration_service_refresh_status();
  if((s_status != CAL_STATUS_VALID) || !s_active_loaded){
    return false;
  }

  rtc_datetime_t now = {};
  if(!datetime_service_get(&now)){
    return false;
  }

  calibration_record_t rec = s_active_cal;
  rec.installation.valid = true;
  rec.installation.timestamp = now;
  rec.installation.mean_mg = s_install_candidate_mean;
  rec.installation.stddev_mg = s_install_candidate_stddev;
  memcpy(rec.installation.matrix, s_install_candidate_matrix, sizeof(rec.installation.matrix));

  if(!calibration_store_save_latest(&rec)){
    return false;
  }

  s_active_cal = rec;
  s_active_loaded = true;
  s_install_session_active = false;
  calibration_service_refresh_status();

  if(out_saved != nullptr){
    *out_saved = rec;
  }

  return true;
}
