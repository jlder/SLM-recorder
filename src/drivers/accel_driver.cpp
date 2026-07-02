// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/accel_driver.cpp
 * @brief Accelerometer driver wrapper used to initialize and read bounded acceleration samples.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/drivers/accel_driver.h"
#include <Arduino.h>
#include <math.h>
#include <SensorQMI8658.hpp>
#include "config.h"
#include "src/services/timebase.h"
#include "src/board/pin_config.h"

static SensorQMI8658 qmi;
static bool s_accel_inited = false;
static accel_calibration_t s_accel_cal = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
static bool s_accel_cal_active = false;
static accel_installation_t s_accel_install = {{1.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f,
                                               0.0f, 0.0f, 1.0f}};
static bool s_accel_install_active = false;

/**
 * Clamp i16 performs the accel driver operation represented by this function
 * and keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: `v`.
 * Returns: Requested numeric value.
 */
static int16_t clamp_i16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

/**
 * Initializes accel drv init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool accel_drv_init(void) {
  if (s_accel_inited) return true;
  const bool ok = qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!ok) {
    return false;
  }

  // Project update: ±8g range
  const int cfg_rc = qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_8G,
                          SensorQMI8658::ACC_ODR_1000Hz,
                          SensorQMI8658::LPF_MODE_0);
  if (cfg_rc != 0) {
    return false;
  }

  const bool en_ok = qmi.enableAccelerometer();
  if (!en_ok) {
    return false;
  }
  s_accel_inited = true;
  return true;
}

/**
 * Accel read xyz performs the accel driver operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */

bool accel_read_temperature_c(float *out_c){
  if((out_c == nullptr) || !s_accel_inited){
    return false;
  }

  const float t = qmi.getTemperature_C();
  if(!isfinite(t)){
    return false;
  }

  *out_c = t;
  return true;
}

bool accel_read_xyz_raw(accel_sample_t *out) {
  if (!out) return false;
  if (!s_accel_inited) return false;

  float ax_g = 0, ay_g = 0, az_g = 0;
  const bool ok = qmi.getAccelerometer(ax_g, ay_g, az_g);
  if (!ok) return false;

  // Convert g -> milli-g (int16), range ±8g => ±8000
  out->ax = clamp_i16((int32_t)lrintf(ax_g * 1000.0f));
  out->ay = clamp_i16((int32_t)lrintf(ay_g * 1000.0f));
  out->az = clamp_i16((int32_t)lrintf(az_g * 1000.0f));
  return true;
}

/**
 * Reads acceleration for normal recorder operation and applies the active
 * gain/offset calibration before returning the sample.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
static void accel_apply_sensor_calibration_(accel_sample_t *out){
  if((out == nullptr) || !s_accel_cal_active){
    return;
  }

  out->ax = clamp_i16((int32_t)lrintf((s_accel_cal.gain_x * (float)out->ax) + s_accel_cal.offset_x_mg));
  out->ay = clamp_i16((int32_t)lrintf((s_accel_cal.gain_y * (float)out->ay) + s_accel_cal.offset_y_mg));
  out->az = clamp_i16((int32_t)lrintf((s_accel_cal.gain_z * (float)out->az) + s_accel_cal.offset_z_mg));
}

static void accel_apply_installation_(accel_sample_t *out){
  if((out == nullptr) || !s_accel_install_active){
    return;
  }

  const float x = (float)out->ax;
  const float y = (float)out->ay;
  const float z = (float)out->az;
  const float *m = s_accel_install.matrix;

  out->ax = clamp_i16((int32_t)lrintf((m[0] * x) + (m[1] * y) + (m[2] * z)));
  out->ay = clamp_i16((int32_t)lrintf((m[3] * x) + (m[4] * y) + (m[5] * z)));
  out->az = clamp_i16((int32_t)lrintf((m[6] * x) + (m[7] * y) + (m[8] * z)));
}

/**
 * Reads acceleration and applies gain/offset sensor correction only.
 * Installation rotation is not applied, so installation calibration can use
 * this function to observe the mounted sensor orientation.
 */
bool accel_read_xyz_sensor_corrected(accel_sample_t *out) {
  if(!accel_read_xyz_raw(out)){
    return false;
  }

  accel_apply_sensor_calibration_(out);
  return true;
}

/**
 * Reads acceleration for normal recorder operation and applies the active
 * gain/offset calibration followed by the installation rotation.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool accel_read_xyz(accel_sample_t *out) {
  if(!accel_read_xyz_sensor_corrected(out)){
    return false;
  }

  accel_apply_installation_(out);
  return true;
}

/**
 * Installs calibration coefficients in the accelerometer driver so subsequent
 * normal reads return corrected samples.
 *
 * Inputs: `cal`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool accel_driver_set_calibration(const accel_calibration_t *cal){
  if(cal == nullptr){
    return false;
  }

  s_accel_cal = *cal;
  s_accel_cal_active = true;
  return true;
}

/**
 * Clears driver calibration coefficients and returns normal acceleration reads
 * to raw/unadjusted values.
 *
 * Inputs: None.
 * Returns: None.
 */
void accel_driver_clear_calibration(void){
  s_accel_cal = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
  s_accel_cal_active = false;
}

bool accel_driver_set_installation(const accel_installation_t *installation){
  if(installation == nullptr){
    return false;
  }

  s_accel_install = *installation;
  s_accel_install_active = true;
  return true;
}

void accel_driver_clear_installation(void){
  s_accel_install = {{1.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, 1.0f}};
  s_accel_install_active = false;
}

/**
 * Accel read xyz bounded performs the accel driver operation represented by
 * this function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `out`, `ts_ms_out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool accel_read_xyz_bounded(accel_sample_t *out, int32_t *ts_ms_out) {
  if (!out || !ts_ms_out) return false;

  // Enforce init-before-use at the bounded API boundary (spec clarity).
  // Still provide a timestamp for the request.
  if (!s_accel_inited) {
    *ts_ms_out = (int32_t)timebase_get_ms_since_midnight();
    return false;
  }

  int32_t ts = 0;
  for (uint32_t i = 0; i < (uint32_t)ACCEL_READ_MAX_TRIES; i++) {
    ts = (int32_t)timebase_get_ms_since_midnight();
    if (accel_read_xyz(out)) {
      *ts_ms_out = ts;
      return true;
    }
  }
  *ts_ms_out = ts;
  return false;
}
