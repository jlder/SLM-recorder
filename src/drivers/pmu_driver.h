// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/pmu_driver.h
 * @brief Public PMU driver API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief PMU driver init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_drv_init(void);
/**
 * @brief PMU shutdown.
 *
 * Inputs: None.
 * Returns: None.
 */
void pmu_shutdown(void);

/**
 * @brief PMU read battery percent.
 *
 * Inputs: `out_percent`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_read_battery_percent(int16_t* out_percent);

/**
 * @brief PMU read USB present.
 *
 * Inputs: `out_present`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_read_usb_present(bool* out_present);

/**
 * @brief PMU is USB connected.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_is_usb_connected(void);

/**
 * @brief PMU battery low.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool pmu_battery_low(void);

