// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/touch_driver.h
 * @brief Public touch driver API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Touch driver init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool touch_drv_init(void);

/**
 * @brief Touch hw get raw.
 *
 * Inputs: `x`, `y`, `pressed`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool touch_hw_get_raw(uint16_t *x, uint16_t *y, bool *pressed);

