// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/display_driver.h
 * @brief Public display driver API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#include <Arduino_GFX_Library.h>
extern "C" {
#endif

/**
 * @brief Display driver init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool display_drv_init(void);

/**
 * @brief Set display brightness.
 *
 * Inputs: `brightness` from 0 to 255.
 * Returns: None.
 */
void display_brightness_set(uint8_t brightness);

/**
 * @brief Enter display standby by switching off the AMOLED panel power path.
 *
 * Inputs: None.
 * Returns: None.
 */
void display_driver_standby_enter(void);

/**
 * @brief Exit display standby and restore the AMOLED panel power path.
 *
 * Inputs: None.
 * Returns: None.
 */
void display_driver_standby_exit(void);

// Accessor for LVGL flush callback
#ifdef __cplusplus
/**
 * @brief Display driver get gfx.
 *
 * Inputs: None.
 * Returns: Pointer to the requested object or string; may be `nullptr` when unavailable.
 */
Arduino_GFX* display_driver_get_gfx(void);
}
#endif
