// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/settings_store.h
 * @brief Public persistent settings API and settings model.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "src/services/language.h"


static const size_t SETTINGS_REGISTRATION_LEN = 6u;
static const size_t SETTINGS_WIFI_PASSWORD_LEN = 32u;

typedef struct {
  char registration[SETTINGS_REGISTRATION_LEN];
  char wifi_password[SETTINGS_WIFI_PASSWORD_LEN]; // generated from registration, not user-editable
  bool date_set;
  bool time_set;
  language_t language;
  bool auto_recording;
  bool auto_wifi;
  bool auto_delete;
} settings_t;

/**
 * Open the Preferences namespace used by the recorder settings.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if the Preferences namespace was opened successfully,
 *   false otherwise.
 */
bool settings_init(void);

/**
 * Copy the current cached settings snapshot into the caller output buffer.
 *
 * Parameters:
 *   out - destination structure that receives the loaded settings.
 *
 * Return:
 *   true if the initialized settings snapshot was copied,
 *   false if storage is not ready or out is null.
 */
bool settings_get(settings_t *out);
language_t settings_get_language(void);

/**
 * Check whether the loaded settings are complete enough for normal operation.
 *
 * Parameters:
 *   in - settings structure to validate.
 *
 * Return:
 *   true if registration, date-set and time-set flags are present,
 *   false otherwise.
 */
bool settings_is_complete(const settings_t *in);

/**
 * Build the deterministic WiFi password from the normalized registration.
 *
 * Parameters:
 *   out    - destination buffer for the generated password.
 *   out_sz - destination buffer size in bytes.
 *   reg    - normalized registration string.
 *
 * Return:
 *   true if the password was generated successfully,
 *   false otherwise.
 */
bool settings_make_wifi_password(char *out, size_t out_sz, const char *reg);

// Runtime settings writes are intentionally not exposed through this public
// read API.  The State task owns all runtime settings changes through the
// private settings_store_write.h interface.
