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

// Settings storage currently uses independent scalar NVS keys in PREFS_NAMESPACE
// (for example "registration", "date_set", and "time_set"). Storage-maintenance rule:
// if a future change alters a key name, value format, stored meaning, or
// converts settings to a packed record, add/bump a dedicated
// SETTINGS_STORAGE_VERSION in config.h and update the load/reject/migration
// handling here. Do not make incompatible persistent settings changes silently.

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
 * Normalize one registration character to the stored registration alphabet.
 *
 * Parameters:
 *   in  - input character.
 *   out - destination normalized character.
 *
 * Return:
 *   true if the character is accepted, false if it shall be ignored.
 */
static bool normalize_registration_char_(char in, char *out){
  if(out == nullptr){
    return false;
  }

  if((in >= '0') && (in <= '9')){
    *out = in;
    return true;
  }

  if((in >= 'a') && (in <= 'z')){
    *out = (char)(in - 'a' + 'A');
    return true;
  }

  if((in >= 'A') && (in <= 'Z')){
    *out = in;
    return true;
  }

  return false;
}

/**
 * Normalize a registration string to five uppercase alphanumeric characters.
 *
 * Parameters:
 *   dst    - destination buffer.
 *   dst_sz - destination buffer size in bytes.
 *   src    - source registration string.
 *
 * Return:
 *   none.
 */
static void sanitize_registration_(char *dst, size_t dst_sz, const char *src){
  if((dst == nullptr) || (dst_sz == 0u)){
    return;
  }

  dst[0] = '\0';
  if(src == nullptr){
    return;
  }

  size_t out_pos = 0u;
  for(size_t in_pos = 0u; (src[in_pos] != '\0') && (out_pos < (dst_sz - 1u)); ++in_pos){
    char normalized = '\0';
    if(normalize_registration_char_(src[in_pos], &normalized)){
      dst[out_pos++] = normalized;
    }
  }
  dst[out_pos] = '\0';
}

/**
 * Check that a stored registration uses the expected fixed-length alphabet.
 *
 * Parameters:
 *   reg - normalized registration string.
 *
 * Return:
 *   true if the registration is valid, false otherwise.
 */
static bool registration_valid_(const char *reg){
  if(reg == nullptr){
    return false;
  }

  for(size_t i = 0u; i < 5u; ++i){
    const char c = reg[i];
    const bool digit = ((c >= '0') && (c <= '9'));
    const bool upper = ((c >= 'A') && (c <= 'Z'));
    if(!(digit || upper)){
      return false;
    }
  }

  return (reg[5] == '\0');
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

  // Read and normalize the registration string stored in flash.
  sanitize_registration_(s_cache.registration,
                         sizeof(s_cache.registration),
                         prefs.getString("registration", "").c_str());

  // The WiFi password is generated from the normalized registration and is not
  // user-editable. Keep the derived value in the cache for status consumers.
  (void)settings_make_wifi_password(s_cache.wifi_password,
                                    sizeof(s_cache.wifi_password),
                                    s_cache.registration);

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
 *   true if registration, date-set and time-set flags are present,
 *   false otherwise.
 */
bool settings_is_complete(const settings_t *in){
  if(in == nullptr){
    return false;
  }

  // Registration must be five uppercase alphanumeric characters.
  if(!registration_valid_(in->registration)){
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
bool settings_make_wifi_password(char *out, size_t out_sz, const char *reg){
  if((out == nullptr) || (out_sz < 9u) || !registration_valid_(reg)){
    if((out != nullptr) && (out_sz > 0u)){
      out[0] = '\0';
    }
    return false;
  }

  out[0] = 'S';
  out[1] = 'L';
  out[2] = 'M';
  for(size_t i = 0u; i < 5u; ++i){
    out[3u + i] = reg[4u - i];
  }
  out[8] = '\0';
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

  char normalized[SETTINGS_REGISTRATION_LEN] = {0};
  sanitize_registration_(normalized, sizeof(normalized), reg);
  if(!registration_valid_(normalized)){
    return false;
  }

  // Update the local cache first, then persist the same normalized value to flash.
  copy_bounded_string(s_cache.registration, sizeof(s_cache.registration), normalized);
  (void)settings_make_wifi_password(s_cache.wifi_password,
                                    sizeof(s_cache.wifi_password),
                                    s_cache.registration);
  return (prefs.putString("registration", s_cache.registration) > 0u);
}

/**
 * Save a Wi-Fi password string.
 *
 * The current access-point password is generated from the registration, so this
 * legacy setter only reports whether settings storage is available.
 *
 * Parameters:
 *   pwd - ignored legacy password string.
 *
 * Return:
 *   true if settings storage is available,
 *   false otherwise.
 */
bool settings_set_wifi_password(const char *pwd){
  (void)pwd;
  return s_storage_ready;
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

  const bool ok = prefs.clear();
  if(ok){
    memset(&s_cache, 0, sizeof(s_cache));
  }
  return ok;
}
