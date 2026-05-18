// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/accel_driver.h
 * @brief Public accelerometer driver API and sample type declarations.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int16_t ax; // milli-g (g*1000)
  int16_t ay;
  int16_t az;
} accel_sample_t;

typedef struct {
  float gain_x;
  float gain_y;
  float gain_z;
  float offset_x_mg;
  float offset_y_mg;
  float offset_z_mg;
} accel_calibration_t;

/**
 * @brief Accel driver init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool accel_drv_init(void);
/**
 * @brief Accel read xyz.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool accel_read_xyz(accel_sample_t *out);

/**
 * Read one raw uncalibrated accelerometer sample.
 *
 * Parameters:
 *   out - destination sample in milli-g.
 *
 * Return:
 *   true if a raw sample was read, false otherwise.
 */
bool accel_read_xyz_raw(accel_sample_t *out);

/**
 * Set the calibration used by normal corrected accelerometer reads.
 *
 * Parameters:
 *   cal - calibration gains and offsets.
 *
 * Return:
 *   true if calibration was accepted, false otherwise.
 */
bool accel_driver_set_calibration(const accel_calibration_t *cal);

/**
 * Clear the active accelerometer calibration.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   none.
 */
void accel_driver_clear_calibration(void);

/**
 * Return whether the accelerometer driver has active calibration.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if calibration is active, false otherwise.
 */
bool accel_driver_has_calibration(void);

// Read one accelerometer sample with bounded retries.
// - Computes a fresh timestamp for every attempt (ts_ms_out = last attempt time).
// - Returns true only when a valid sample is read into *out.
/**
 * @brief Accel read xyz bounded.
 *
 * Inputs: `out`, `ts_ms_out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool accel_read_xyz_bounded(accel_sample_t *out, int32_t *ts_ms_out);

