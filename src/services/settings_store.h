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
#include <Preferences.h>

extern Preferences prefs;

static const size_t SETTINGS_REGISTRATION_LEN = 6u;
static const size_t SETTINGS_WIFI_PASSWORD_LEN = 32u;

typedef struct {
  char registration[SETTINGS_REGISTRATION_LEN];
  char wifi_password[SETTINGS_WIFI_PASSWORD_LEN];
  bool date_set;
  bool time_set;
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
 * Load the current settings from Preferences into the caller output buffer.
 *
 * Parameters:
 *   out - destination structure that receives the loaded settings.
 *
 * Return:
 *   true if settings were loaded successfully,
 *   false if storage is not ready or out is null.
 */
bool settings_get(settings_t *out);

/**
 * Check whether the loaded settings are complete enough for normal operation.
 *
 * Parameters:
 *   in - settings structure to validate.
 *
 * Return:
 *   true if registration, Wi-Fi password, date-set and time-set flags are present,
 *   false otherwise.
 */
bool settings_is_complete(const settings_t *in);

// NOTE: These functions write to NVS/flash via Preferences.
// Project policy: only the State task shall call these setters.

/**
 * Save the registration string.
 *
 * Parameters:
 *   reg - null-terminated registration string to store.
 *
 * Return:
 *   true if the value was written successfully,
 *   false otherwise.
 */
bool settings_set_registration(const char *reg);

/**
 * Save the Wi-Fi password string.
 *
 * Parameters:
 *   pwd - null-terminated Wi-Fi password string to store.
 *
 * Return:
 *   true if the value was written successfully,
 *   false otherwise.
 */
bool settings_set_wifi_password(const char *pwd);

/**
 * Save the flag that indicates whether the date was set by the user.
 *
 * Parameters:
 *   done - true when the date has been configured, false otherwise.
 *
 * Return:
 *   true if the value was written successfully,
 *   false otherwise.
 */
bool settings_set_date_set(bool done);

/**
 * Save the flag that indicates whether the time was set by the user.
 *
 * Parameters:
 *   done - true when the time has been configured, false otherwise.
 *
 * Return:
 *   true if the value was written successfully,
 *   false otherwise.
 */
bool settings_set_time_set(bool done);
/**
 * Clear all recorder settings stored in the Preferences namespace.
 *
 * Parameters:
 *   none
 *
 * Return:
 *   true if all keys in the namespace were cleared successfully,
 *   false otherwise.
 */
bool settings_clear(void);
