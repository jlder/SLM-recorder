// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/ui_message.cpp
 * @brief User-interface message lookup table.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/ui_message.h"

typedef struct {
  msg_id_t id;
  const char *text;
  ui_severity_t severity;
  ui_color_t color;
  bool force_main;
  bool blink;
} row_t;

// Central message policy table.
// NOTE: Keep strings short and deterministic (no dynamic formatting).
static const row_t kTable[] = {
  { MSG_NONE, "", UI_SEV_INFO, UI_COLOR_DEFAULT, false, false },

  // Startup / nominal
  { MSG_BOOT, "BOOT", UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_READY, "READY", UI_SEV_INFO, UI_COLOR_GREEN, false, false },
  { MSG_RECORDING, "RECORDING", UI_SEV_INFO, UI_COLOR_GREEN, false, false },
  { MSG_STARTING, "STARTING", UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_STOPPING, "STOPPING", UI_SEV_INFO, UI_COLOR_GREEN, true, false },

  // Transient / config
  { MSG_OFF, "OFF", UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_RETRY, "RETRY", UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_WIFI_OFF, "WIFI OFF", UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_REG_SAVED, "REG SAVED", UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_PWD_SAVED, "PWD SAVED", UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_SETTINGS_LOCKED, "NEED SETTINGS", UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_CALIBRATION_REQUIRED, "CAL REQUIRED", UI_SEV_WARN, UI_COLOR_AMBER, false, true },
  { MSG_CALIBRATION_FAULT, "CAL FAULT", UI_SEV_ERROR, UI_COLOR_RED, false, true },

  // Hardware errors
  { MSG_ACCEL_ERROR, "ACCEL ERR", UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_RTC_ERROR, "RTC ERROR", UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_NO_RTC_DATE_TIME, "NO RTC", UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_PMU_ERROR, "PMU ERROR", UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_RECORD_FAIL, "REC FAIL", UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_TOUCH_ERROR, "TOUCH ERROR", UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_ERROR, "ERROR", UI_SEV_ERROR, UI_COLOR_RED, true, true },


  // SD / storage
  { MSG_NO_SD, "NO SD", UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_SD_LOW_SPACE, "SD LOW", UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_SD_FULL, "SD FULL", UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_SD_FULL_FILES, "SD FULL (FILES)", UI_SEV_WARN, UI_COLOR_AMBER, false, true },
  { MSG_SD_ERROR, "SD ERROR", UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_SD_OK_CLR, "SD OK/CLR", UI_SEV_INFO, UI_COLOR_GREEN, true, false },

  // Power / shutdown
  { MSG_LOW_BATT, "LOW BATT", UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_USB_LOST, "USB LOST", UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_SHUTDOWN, "SHUTDOWN", UI_SEV_ERROR, UI_COLOR_RED, true, true },

  // Fatal
  { MSG_FATAL, "GENERIC ERROR", UI_SEV_ERROR, UI_COLOR_RED, true, true },
};

static ui_message_info_t s_info; // returned pointer refers to this stable object

/**
 * @brief Find the UI message table row for an identifier.
 *
 * Inputs: `id`.
 * Returns: Pointer to the matching row, or `nullptr` if not found.
 */
static const row_t *find_row(msg_id_t id){
  for (unsigned i = 0; i < (unsigned)(sizeof(kTable)/sizeof(kTable[0])); i++){
    if (kTable[i].id == id) return &kTable[i];
  }
  return &kTable[0];
}

/**
 * @brief Return UI message metadata for a message identifier.
 *
 * Inputs: `id`.
 * Returns: Pointer to message metadata, or `nullptr` if the identifier is unknown.
 */
const ui_message_info_t *ui_message_get(msg_id_t id){
  const row_t *r = find_row(id);
  s_info.id = r->id;
  s_info.text = r->text;
  s_info.severity = r->severity;
  s_info.color = r->color;
  s_info.force_main = r->force_main;
  s_info.blink = r->blink;
    return &s_info;
}


