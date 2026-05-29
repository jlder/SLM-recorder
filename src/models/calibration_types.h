// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/models/calibration_types.h
 * @brief Calibration data types shared by calibration storage, services, and record formatting.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "src/drivers/rtc_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CAL_FACE_PX = 0,
  CAL_FACE_NX,
  CAL_FACE_PY,
  CAL_FACE_NY,
  CAL_FACE_PZ,
  CAL_FACE_NZ,
  CAL_FACE_COUNT
} calibration_face_t;

typedef struct {
  float x_mg;
  float y_mg;
  float z_mg;
} calibration_vec_t;

typedef struct {
  bool valid;
  calibration_vec_t mean_mg;
  calibration_vec_t stddev_mg;
} calibration_face_capture_t;

typedef struct {
  bool valid;
  rtc_datetime_t timestamp;
  calibration_vec_t mean_mg;
  calibration_vec_t stddev_mg;
  float matrix[9];
} installation_calibration_t;

typedef struct {
  bool valid;
  rtc_datetime_t timestamp;

  float gravity_mg;

  float gain_x;
  float gain_y;
  float gain_z;

  float offset_x_mg;
  float offset_y_mg;
  float offset_z_mg;

  calibration_face_capture_t face[CAL_FACE_COUNT];
} sensor_calibration_t;

typedef struct {
  uint32_t version;

  sensor_calibration_t sensor;
  installation_calibration_t installation;

  uint32_t checksum;
} calibration_record_t;

typedef enum {
  CAL_STATUS_MISSING = 0,
  CAL_STATUS_VALID,
  CAL_STATUS_EXPIRED,
  CAL_STATUS_FAULT
} calibration_status_t;

#ifdef __cplusplus
}
#endif
