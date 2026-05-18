// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/settings_store.cpp
 * @brief Persistent settings storage backed by ESP Preferences.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/settings_store.h"
#include <string.h>
#include "config.h"

Preferences prefs;

// Keep one local copy of the settings so getters and setters use the same data shape.
static settings_t s_cache = {"", "", false, false};

// Latch whether the Preferences namespace is available.
static bool s_storage_ready = false;

/**
 * Copy a C string into a fixed-size destination buffer with guaranteed termination.
 *
 * Parameters:
 *   dst    - destination buffer.
 *   dst_sz - destination buffer size in bytes.
 *   src    - source string, may be null.
 *
 * Return:
 *   none.
 */
static void copy_bounded_string(char *dst, size_t dst_sz, const char *src){
  if((dst == nullptr) || (dst_sz == 0u)){
    return;
  }

  // Treat a null source as an empty string.
  if(src == nullptr){
    dst[0] = '\0';
    return;
  }

  // Copy at most dst_sz - 1 characters and force a trailing null byte.
  strncpy(dst, src, dst_sz - 1u);
  dst[dst_sz - 1u] = '\0';
}

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
bool settings_init(void){
  s_storage_ready = prefs.begin(PREFS_NAMESPACE, false);
  return s_storage_ready;
}

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
bool settings_get(settings_t *out){
  if((out == nullptr) || !s_storage_ready){
    return false;
  }

  // Read the registration string stored in flash.
  copy_bounded_string(s_cache.registration,
                      sizeof(s_cache.registration),
                      prefs.getString("registration", "").c_str());

  // Read the Wi-Fi password string stored in flash.
  copy_bounded_string(s_cache.wifi_password,
                      sizeof(s_cache.wifi_password),
                      prefs.getString("wifi_pwd", "").c_str());

  // Read whether the user has already set the date and time at least once.
  s_cache.date_set = prefs.getBool("date_set", false);
  s_cache.time_set = prefs.getBool("time_set", false);

  // Return the loaded cache to the caller.
  *out = s_cache;
  return true;
}

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
bool settings_is_complete(const settings_t *in){
  if(in == nullptr){
    return false;
  }

  // Registration must not be empty.
  if(in->registration[0] == '\0'){
    return false;
  }

  // Wi-Fi password must not be empty.
  if(in->wifi_password[0] == '\0'){
    return false;
  }

  // Date must have been set by the user.
  if(!in->date_set){
    return false;
  }

  // Time must have been set by the user.
  if(!in->time_set){
    return false;
  }

  return true;
}

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
bool settings_set_registration(const char *reg){
  if((reg == nullptr) || !s_storage_ready){
    return false;
  }

  // Update the local cache first, then persist the same value to flash.
  copy_bounded_string(s_cache.registration, sizeof(s_cache.registration), reg);
  return (prefs.putString("registration", s_cache.registration) > 0u);
}

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
bool settings_set_wifi_password(const char *pwd){
  if((pwd == nullptr) || !s_storage_ready){
    return false;
  }

  // Update the local cache first, then persist the same value to flash.
  copy_bounded_string(s_cache.wifi_password, sizeof(s_cache.wifi_password), pwd);
  return (prefs.putString("wifi_pwd", s_cache.wifi_password) > 0u);
}

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
bool settings_set_date_set(bool done){
  if(!s_storage_ready){
    return false;
  }

  // Update the cached flag and persist the same value to flash.
  s_cache.date_set = done;
  return (prefs.putBool("date_set", done) > 0u);
}

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
bool settings_set_time_set(bool done){
  if(!s_storage_ready){
    return false;
  }

  // Update the cached flag and persist the same value to flash.
  s_cache.time_set = done;
  return (prefs.putBool("time_set", done) > 0u);
}

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
bool settings_clear(void){
  if(!s_storage_ready){
    return false;
  }

  return prefs.clear();
}
