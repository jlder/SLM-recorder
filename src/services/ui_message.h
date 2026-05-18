// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/ui_message.h
 * @brief Public UI message lookup API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "src/models/system_message.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  UI_SEV_INFO = 0,
  UI_SEV_WARN = 1,
  UI_SEV_ERROR = 2
} ui_severity_t;

typedef enum {
  UI_COLOR_DEFAULT = 0,
  UI_COLOR_GREEN,
  UI_COLOR_AMBER,
  UI_COLOR_RED
} ui_color_t;

typedef struct {
  msg_id_t id;
  const char *text;

  ui_severity_t severity;
  ui_color_t color;

  bool force_main;   // force UI back to main screen
  bool blink;        // optional (UI may implement)
} ui_message_info_t;

// Get message metadata by ID. Always returns a valid pointer (MSG_NONE -> empty).
const ui_message_info_t *ui_message_get(msg_id_t id);


#ifdef __cplusplus
}
#endif
