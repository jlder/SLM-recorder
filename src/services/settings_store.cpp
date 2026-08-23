// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/settings_store.cpp
 * @brief Persistent settings storage backed by ESP Preferences.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/settings_store_write.h"
#include <string.h>
#include <Preferences.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "config.h"

static Preferences s_prefs;
static portMUX_TYPE s_cache_mux = portMUX_INITIALIZER_UNLOCKED;

// Settings storage currently uses independent scalar NVS keys in PREFS_NAMESPACE
// (for example "registration", "date_set", and "time_set"). Storage-maintenance rule:
// if a future change alters a key name, value format, stored meaning, or
// converts settings to a packed record, add/bump a dedicated
// SETTINGS_STORAGE_VERSION in config.h and update the load/reject/migration
// handling here. Do not make incompatible persistent settings changes silently.

// Keep one local copy of the settings so getters and setters use the same data shape.
static settings_t s_cache = {"", "", false, false, LANGUAGE_FRENCH, false, false, false};

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
  s_storage_ready = s_prefs.begin(PREFS_NAMESPACE, false);
  if(!s_storage_ready){
    return false;
  }

  // Firmware/schema migration is a boot-time initialization operation performed
  // before recorder tasks start. Runtime writes are State-task-only.
  const String automation_firmware = s_prefs.getString("automation_fw", "");
  const uint32_t automation_schema = s_prefs.getUInt("auto_schema", 0u);
  if((automation_firmware != String(RECORDER_SOFTWARE_VERSION)) ||
     (automation_schema != AUTOMATION_SETTINGS_SCHEMA_VERSION)){
    (void)s_prefs.putBool("auto_record", false);
    (void)s_prefs.putBool("auto_wifi", false);
    (void)s_prefs.putBool("auto_delete", false);
    (void)s_prefs.putString("automation_fw", RECORDER_SOFTWARE_VERSION);
    (void)s_prefs.putUInt("auto_schema", AUTOMATION_SETTINGS_SCHEMA_VERSION);
  }

  settings_t loaded = {"", "", false, false, LANGUAGE_FRENCH, false, false, false};
  sanitize_registration_(loaded.registration,
                         sizeof(loaded.registration),
                         s_prefs.getString("registration", "").c_str());
  (void)settings_make_wifi_password(loaded.wifi_password,
                                    sizeof(loaded.wifi_password),
                                    loaded.registration);
  loaded.date_set = s_prefs.getBool("date_set", false);
  loaded.time_set = s_prefs.getBool("time_set", false);
  const uint8_t stored_language = s_prefs.getUChar("language", (uint8_t)LANGUAGE_FRENCH);
  loaded.language = language_valid((language_t)stored_language)
                      ? (language_t)stored_language
                      : LANGUAGE_FRENCH;
  loaded.auto_recording = s_prefs.getBool("auto_record", false);
  loaded.auto_wifi = s_prefs.getBool("auto_wifi", false);
  loaded.auto_delete = s_prefs.getBool("auto_delete", false);

  portENTER_CRITICAL(&s_cache_mux);
  s_cache = loaded;
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
}

language_t settings_get_language(void){
  language_t language = LANGUAGE_FRENCH;
  portENTER_CRITICAL(&s_cache_mux);
  language = s_cache.language;
  portEXIT_CRITICAL(&s_cache_mux);
  return language_valid(language) ? language : LANGUAGE_FRENCH;
}

/**
 * Load the current settings snapshot into the caller output buffer.
 *
 * NVS is read only during settings_init(). Runtime readers consume the cached
 * snapshot; the State task is the sole runtime writer.
 */
bool settings_get(settings_t *out){
  if((out == nullptr) || !s_storage_ready){
    return false;
  }

  portENTER_CRITICAL(&s_cache_mux);
  *out = s_cache;
  portEXIT_CRITICAL(&s_cache_mux);
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

  char password[SETTINGS_WIFI_PASSWORD_LEN] = {0};
  (void)settings_make_wifi_password(password, sizeof(password), normalized);
  if(s_prefs.putString("registration", normalized) == 0u){
    return false;
  }

  portENTER_CRITICAL(&s_cache_mux);
  copy_bounded_string(s_cache.registration, sizeof(s_cache.registration), normalized);
  copy_bounded_string(s_cache.wifi_password, sizeof(s_cache.wifi_password), password);
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
}

bool settings_set_wifi_password(const char *pwd){
  (void)pwd;
  return s_storage_ready;
}

bool settings_set_date_set(bool done){
  if(!s_storage_ready || (s_prefs.putBool("date_set", done) == 0u)){
    return false;
  }
  portENTER_CRITICAL(&s_cache_mux);
  s_cache.date_set = done;
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
}

bool settings_set_time_set(bool done){
  if(!s_storage_ready || (s_prefs.putBool("time_set", done) == 0u)){
    return false;
  }
  portENTER_CRITICAL(&s_cache_mux);
  s_cache.time_set = done;
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
}

bool settings_set_language(language_t language){
  if(!s_storage_ready || !language_valid(language) ||
     (s_prefs.putUChar("language", (uint8_t)language) == 0u)){
    return false;
  }
  portENTER_CRITICAL(&s_cache_mux);
  s_cache.language = language;
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
}

bool settings_set_auto_recording(bool enabled){
  if(!s_storage_ready || (s_prefs.putBool("auto_record", enabled) == 0u)){
    return false;
  }
  portENTER_CRITICAL(&s_cache_mux);
  s_cache.auto_recording = enabled;
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
}

bool settings_set_auto_wifi(bool enabled){
  if(!s_storage_ready || (s_prefs.putBool("auto_wifi", enabled) == 0u)){
    return false;
  }
  portENTER_CRITICAL(&s_cache_mux);
  s_cache.auto_wifi = enabled;
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
}

bool settings_set_auto_delete(bool enabled){
  if(!s_storage_ready || (s_prefs.putBool("auto_delete", enabled) == 0u)){
    return false;
  }
  portENTER_CRITICAL(&s_cache_mux);
  s_cache.auto_delete = enabled;
  portEXIT_CRITICAL(&s_cache_mux);
  return true;
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

  const bool ok = s_prefs.clear();
  if(ok){
    settings_t cleared = {"", "", false, false, LANGUAGE_FRENCH, false, false, false};
    portENTER_CRITICAL(&s_cache_mux);
    s_cache = cleared;
    portEXIT_CRITICAL(&s_cache_mux);
  }
  return ok;
}
