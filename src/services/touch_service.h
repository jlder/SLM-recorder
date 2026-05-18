// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/touch_service.h
 * @brief Public touch-service API and snapshot type.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint16_t x;
  uint16_t y;
  bool     pressed;
  bool     valid;
} touch_snapshot_t;

/**
 * @brief Report whether touch sampling is currently enabled.
 *
 * Inputs: None.
 * Returns: `true` when touch sampling is enabled; otherwise `false`.
 */
bool touch_is_enabled(void);

/**
 * @brief Touch enable.
 *
 * Inputs: `enabled`.
 * Returns: None.
 */
void touch_enable(bool enabled);

/**
 * @brief Touch service update from hw.
 *
 * Inputs: None.
 * Returns: None.
 */
void touch_service_update_from_hw(void);

/**
 * @brief Touch service get snapshot.
 *
 * Inputs: None.
 * Returns: Current touch snapshot.
 */
touch_snapshot_t touch_service_get_snapshot(void);
