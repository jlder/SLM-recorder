// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/device_service.h
 * @brief Public device-service API for drivers and device status.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

extern bool i2c_ok;
extern bool pmu_ok;
extern bool rtc_ok;
extern bool touch_ok;
extern bool accel_ok;
extern bool display_ok;

/**
 * @brief I2C init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool i2c_init(void);
/**
 * @brief PMU init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_init(void);
/**
 * @brief RTC init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool rtc_init(void);
/**
 * @brief Touch init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool touch_init(void);
/**
 * @brief Accel init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool accel_init(void);
/**
 * @brief Display init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool display_init(void);

/**
 * @brief USB present.
 *
 * Inputs: `out_present`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool usb_present(bool* out_present);
/**
 * @brief Battery percent.
 *
 * Inputs: `out_percent`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool battery_percent(uint8_t* out_percent);
/**
 * @brief Shutdown device.
 *
 * Inputs: None.
 * Returns: None.
 */
void shutdown_device(void);
