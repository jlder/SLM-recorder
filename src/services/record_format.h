// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/record_format.h
 * @brief Public record-format structures and helper API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "src/services/ring_buffer.h"
#include "src/drivers/accel_driver.h"
#include "src/models/calibration_types.h"



typedef struct __attribute__((packed)) {
  uint8_t  sync;
  uint8_t  id;
  uint16_t size;

  uint32_t calibration_version;

  uint16_t year;
  uint8_t  month;
  uint8_t  day;
  uint8_t  hour;
  uint8_t  minute;
  uint8_t  second;

  float gain_x;
  float gain_y;
  float gain_z;

  float offset_x_mg;
  float offset_y_mg;
  float offset_z_mg;

  float face_mean_mg[CAL_FACE_COUNT][3];
  float face_stddev_mg[CAL_FACE_COUNT][3];

  uint8_t installation_valid;
  uint16_t installation_year;
  uint8_t installation_month;
  uint8_t installation_day;
  uint8_t installation_hour;
  uint8_t installation_minute;
  uint8_t installation_second;
  float installation_mean_mg[3];
  float installation_stddev_mg[3];
  float installation_matrix[9];

  uint8_t checksum;
} record_calibration_block_t;

typedef struct __attribute__((packed)) {
  uint8_t  sync;
  uint8_t  id;
  uint16_t overflow;
  uint16_t reserved1;
  uint16_t reserved2;
  uint16_t reserved3;
  uint16_t reserved4;
  uint8_t  checksum;
} record_status_block_t;

/**
 * @brief Record format build block.
 *
 * Inputs: `out`, `ts_ms`, `sample`.
 * Returns: None.
 */
void record_format_build_block(record_block_t *out, int32_t ts_ms, const accel_sample_t *sample);
/**
 * @brief Record filename.
 *
 * Inputs: `out`, `out_sz`, `registration`, `datetime_token`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool record_filename(char *out, size_t out_sz, const char *registration, const char *datetime_token);
/**
 * @brief Record format build status block.
 *
 * Inputs: `out`, `overflow_count`.
 * Returns: None.
 */
void record_format_build_status_block(record_status_block_t *out, uint32_t overflow_count);

/**
 * @brief Record format build calibration block.
 *
 * Inputs: `out`, `calibration`.
 * Returns: None.
 */
void record_format_build_calibration_block(record_calibration_block_t *out,
                                           const calibration_record_t *calibration);
