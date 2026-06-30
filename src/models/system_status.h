// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/models/system_status.h
 * @brief Snapshot model for user-visible recorder state and device status.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "src/models/system_state.h"
#include "src/models/system_message.h"

typedef struct {
  recorder_state_t state;
  uint8_t  battery_percent;
  bool     battery_percent_valid;
  bool     usb_present;
  bool     usb_present_valid;
  // Diagnostic SD-present snapshot used for watchdog fault reporting.
  // SD capacity/free-space for Web file management is queried through
  // sd_files_get_space() and /api/info, not through system_status_t.
  bool     sd_present;
  bool     wifi_active;
  int32_t  last_error;
  msg_id_t message_id;
} system_status_t;
