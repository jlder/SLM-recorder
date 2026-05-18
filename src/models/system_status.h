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
  // Pulse set true for one state_task cycle when USB power was previously present and is now absent.
  bool     usb_lost;
  bool     sd_present;
  uint32_t sd_total_mb;
  uint32_t sd_used_mb;
  uint32_t sd_free_mb;
  bool     wifi_active;
  int32_t  last_error;
  msg_id_t message_id;
} system_status_t;
