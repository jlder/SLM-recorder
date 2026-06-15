// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/models/system_message.h
 * @brief Message identifier model shared by state, UI, and error display logic.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// System-visible message identifiers. UI renders these via a central table.
typedef enum {
  MSG_NONE = 0,

  // Startup / nominal
  MSG_BOOT,
  MSG_READY,
  MSG_RECORDING,
  MSG_STARTING,
  MSG_STOPPING,

  // Transient / config
  MSG_ERROR,
  MSG_SETTINGS_LOCKED,
  MSG_ACCEL_CALIBRATION_REQUIRED,
  MSG_INSTALLATION_CALIBRATION_REQUIRED,
  MSG_CALIBRATION_FAULT,

  // Hardware errors
  MSG_ACCEL_ERROR,
  MSG_RTC_ERROR,
  MSG_PMU_ERROR,
  MSG_RECORD_FAIL,
  MSG_TOUCH_ERROR,

  // SD / storage
  MSG_NO_SD,
  MSG_SD_ERROR,
  // SD file-count limit reached (refuse to start recording).
  MSG_SD_FULL_FILES,
  MSG_SD_LOW_SPACE,
  // SD recovered (card inserted / space OK) while still in ERROR; prompts operator clear.
  MSG_SD_OK_CLR,

  // Power / shutdown
  MSG_LOW_BATT,
  MSG_SHUTDOWN,

  // Generic fatal
  MSG_FATAL,
  MSG_FATAL_WDG_CLR,

  MSG__COUNT
} msg_id_t;


#ifdef __cplusplus
}
#endif
