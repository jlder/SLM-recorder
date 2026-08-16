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
#include "src/services/language.h"
#include "src/services/settings_store.h"

typedef struct {
  msg_id_t id;
  language_text_id_t text_id;
  ui_severity_t severity;
  ui_color_t color;
  bool force_main;
  bool blink;
} row_t;

// Central message policy table.
// NOTE: Keep strings short and deterministic (no dynamic formatting).
static const row_t kTable[] = {
  { MSG_NONE, TXT_EMPTY, UI_SEV_INFO, UI_COLOR_DEFAULT, false, false },

  // Startup / nominal
  { MSG_BOOT, TXT_BOOT, UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_READY, TXT_READY, UI_SEV_INFO, UI_COLOR_GREEN, false, false },
  { MSG_RECORDING, TXT_RECORDING, UI_SEV_INFO, UI_COLOR_GREEN, false, false },
  { MSG_STARTING, TXT_STARTING, UI_SEV_INFO, UI_COLOR_GREEN, true, false },
  { MSG_STOPPING, TXT_STOPPING, UI_SEV_INFO, UI_COLOR_GREEN, true, false },

  // Transient / config
  { MSG_SETTINGS_LOCKED, TXT_NEED_SETTINGS, UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_ACCEL_CALIBRATION_REQUIRED, TXT_REC_CAL_REQ, UI_SEV_WARN, UI_COLOR_AMBER, false, true },
  { MSG_INSTALLATION_CALIBRATION_REQUIRED, TXT_INST_CAL_REQ, UI_SEV_WARN, UI_COLOR_AMBER, false, true },
  { MSG_CALIBRATION_FAULT, TXT_REC_CAL_FAULT, UI_SEV_ERROR, UI_COLOR_RED, false, true },

  // Hardware errors
  { MSG_ACCEL_ERROR, TXT_ACCEL_ERR, UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_RTC_ERROR, TXT_RTC_ERROR, UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_PMU_ERROR, TXT_PMU_ERROR, UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_RECORD_FAIL, TXT_REC_FAIL, UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_TOUCH_ERROR, TXT_TOUCH_ERROR, UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_ERROR, TXT_ERROR, UI_SEV_ERROR, UI_COLOR_RED, true, true },


  // SD / storage
  { MSG_NO_SD, TXT_NO_SD, UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_SD_LOW_SPACE, TXT_SD_LOW, UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_SD_FULL_FILES, TXT_SD_FULL_FILES, UI_SEV_WARN, UI_COLOR_AMBER, false, true },
  { MSG_SD_ERROR, TXT_SD_FILE_ERR, UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_SD_OK_CLR, TXT_SD_OK_CLR, UI_SEV_INFO, UI_COLOR_GREEN, true, false },

  // Power / shutdown
  { MSG_LOW_BATT, TXT_LOW_BATT, UI_SEV_WARN, UI_COLOR_AMBER, true, true },
  { MSG_SHUTDOWN, TXT_SHUTDOWN, UI_SEV_INFO, UI_COLOR_GREEN, true, false },

  // Fatal
  { MSG_FATAL, TXT_GENERIC_ERROR, UI_SEV_ERROR, UI_COLOR_RED, true, true },
  { MSG_FATAL_WDG_CLR, TXT_FATAL_WDG_CLR, UI_SEV_ERROR, UI_COLOR_RED, true, true },
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
  s_info.text = language_text(r->text_id, settings_get_language());
  s_info.severity = r->severity;
  s_info.color = r->color;
  s_info.force_main = r->force_main;
  s_info.blink = r->blink;
    return &s_info;
}


