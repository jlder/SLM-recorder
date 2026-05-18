// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/i2c_bus.h
 * @brief Public I2C bus initialization API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief I2C bus driver init.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool i2c_bus_drv_init(void);
