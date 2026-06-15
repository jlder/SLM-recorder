// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/watchdog_service.cpp
 * @brief Software watchdog supervising critical recorder task heartbeats.
 */

#include "src/services/watchdog_service.h"

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <esp_timer.h>

#include "config.h"
#include "src/models/system_status.h"
#include "src/tasks/state_task.h"
#include "src/services/device_service.h"

typedef struct {
  // Last heartbeat value written by the monitored source.  It is a timestamp,
  // but the watchdog decision uses it only as a progress token: changed means
  // the source executed since the previous checker pass.
  int64_t last_us;

  // Last heartbeat value observed by watchdog_service_check().
  int64_t seen_us;

  // Number of consecutive checker periods with no heartbeat progress.
  uint8_t missed_checks;

  // When false, the source is not expected to run and cannot trip the watchdog.
  bool required;
} watchdog_entry_t;

static Preferences s_prefs;
static bool s_prefs_open = false;
static bool s_fault_handling = false;
static watchdog_entry_t s_wd[WD_COUNT];

// Persistent diagnostic format version.  Older records are ignored so stale
// NVS data from previous watchdog algorithms cannot be displayed as a current
// Health-page diagnostic after a firmware update.
static const uint8_t WATCHDOG_DIAG_RECORD_VERSION = 1u;

static portMUX_TYPE s_wd_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Returns the number of consecutive unchanged checker observations required to
 * declare a watchdog timeout.
 *
 * The decision is progress-based, not age-based.  The +1 margin prevents an
 * early timeout when a source stops immediately before a checker pass.
 *
 * Inputs: None.
 * Returns: Minimum number of missed checker periods.
 */
static uint8_t watchdog_missed_limit_(void){
  const uint32_t period_ms = (WATCHDOG_CHECK_PERIOD_MS > 0u) ?
      (uint32_t)WATCHDOG_CHECK_PERIOD_MS : 1u;
  uint32_t limit = ((uint32_t)WATCHDOG_TIMEOUT_MS + period_ms - 1u) / period_ms;
  limit += 1u;
  if(limit > 255u){
    limit = 255u;
  }
  return (uint8_t)limit;
}

/**
 * Converts an elapsed time in microseconds to milliseconds for NVS/Web
 * reporting only.
 *
 * Inputs: `age_us` elapsed time, normally now_us - last_us.
 * Returns: Milliseconds clipped to uint32_t range.
 */
static uint32_t watchdog_elapsed_us_to_ms_(int64_t age_us){
  if(age_us <= 0){
    return 0u;
  }

  const int64_t age_ms = age_us / 1000LL;
  if(age_ms > (int64_t)UINT32_MAX){
    return UINT32_MAX;
  }
  return (uint32_t)age_ms;
}

/**
 * Returns a short printable name for a watchdog source.
 *
 * Inputs: `source`.
 * Returns: Constant source name string.
 */
static const char *watchdog_source_name_(watchdog_source_t source){
  switch(source){
    case WD_STATE:  return "state";
    case WD_SD:     return "sd";
    case WD_RECORD: return "record";
    case WD_WEB:    return "web";
    default:        return "unknown";
  }
}

/**
 * Reports whether the persistent watchdog diagnostic record matches the
 * current format.
 *
 * Inputs: None.
 * Returns: `true` when the stored diagnostic can be safely decoded.
 */
static bool watchdog_persistent_diag_valid_(void){
  if(!s_prefs_open){
    return false;
  }

  if(!s_prefs.getBool("valid", false)){
    return false;
  }

  return s_prefs.getUChar("diag_ver", 0u) == WATCHDOG_DIAG_RECORD_VERSION;
}

/**
 * Stores persistent watchdog fault details in NVS.
 *
 * The active fault flag is separate from the diagnostic snapshot.  Clearing the
 * device UI warning removes only the active flag; the snapshot remains
 * available to the Web maintenance page.
 *
 * Inputs: `source`, `age_ms`, `ages_ms`, `st`.
 * Returns: None.
 */
static void watchdog_persistent_fault_set_(watchdog_source_t source,
                                           uint32_t age_ms,
                                           const uint32_t ages_ms[WD_COUNT],
                                           const system_status_t *st){
  if(!s_prefs_open){
    return;
  }

  (void)s_prefs.putBool(WATCHDOG_PREFS_KEY, true);
  (void)s_prefs.putBool("valid", true);
  (void)s_prefs.putUChar("diag_ver", WATCHDOG_DIAG_RECORD_VERSION);
  (void)s_prefs.putUChar("src", (uint8_t)source);
  (void)s_prefs.putUInt("age", age_ms);

  if(ages_ms != nullptr){
    (void)s_prefs.putUInt("age_st", ages_ms[WD_STATE]);
    (void)s_prefs.putUInt("age_sd", ages_ms[WD_SD]);
    (void)s_prefs.putUInt("age_rec", ages_ms[WD_RECORD]);
    (void)s_prefs.putUInt("age_web", ages_ms[WD_WEB]);
  }

  if(st != nullptr){
    (void)s_prefs.putUInt("state", (uint32_t)st->state);
    (void)s_prefs.putInt("err", (int32_t)st->last_error);
    (void)s_prefs.putBool("web", st->wifi_active);
    (void)s_prefs.putBool("usb", st->usb_present_valid && st->usb_present);
    (void)s_prefs.putBool("sd", st->sd_present);
  }

  (void)s_prefs.putUInt("heap", (uint32_t)ESP.getFreeHeap());
  (void)s_prefs.putUInt("minheap", (uint32_t)ESP.getMinFreeHeap());
}

/**
 * Checks whether a watchdog source is valid.
 *
 * Inputs: `source`.
 * Returns: `true` when source is in range.
 */
static bool watchdog_source_valid_(watchdog_source_t source){
  return ((int)source >= 0) && (source < WD_COUNT);
}

/**
 * Initializes software watchdog runtime state and opens persistent fault
 * storage.
 *
 * Inputs: None.
 * Returns: None.
 */
void watchdog_service_init(void){
  const int64_t now_us = esp_timer_get_time();

  portENTER_CRITICAL(&s_wd_mux);
  for(uint8_t i = 0u; i < (uint8_t)WD_COUNT; ++i){
    s_wd[i].last_us = now_us;
    s_wd[i].seen_us = now_us;
    s_wd[i].missed_checks = 0u;
    s_wd[i].required = false;
  }

  // State and SD tasks are expected to run continuously after setup starts them.
  s_wd[WD_STATE].required = true;
  s_wd[WD_SD].required = true;
  s_wd[WD_RECORD].required = false;
  s_wd[WD_WEB].required = false;
  s_fault_handling = false;
  portEXIT_CRITICAL(&s_wd_mux);

  s_prefs_open = s_prefs.begin(WATCHDOG_PREFS_NAMESPACE, false);
}

/**
 * Records heartbeat progress for one monitored source.
 *
 * The timestamp is used as a progress token.  The checker does not subtract a
 * separately sampled task timestamp for the timeout decision; it only verifies
 * that this value changed between checker passes.
 *
 * Inputs: `source`.
 * Returns: None.
 */
void watchdog_kick(watchdog_source_t source){
  if(!watchdog_source_valid_(source)){
    return;
  }

  const int64_t now_us = esp_timer_get_time();

  portENTER_CRITICAL(&s_wd_mux);
  s_wd[source].last_us = now_us;
  portEXIT_CRITICAL(&s_wd_mux);
}

/**
 * Enables or disables timeout enforcement for a monitored source.
 *
 * Enabling a source starts a fresh observation window.  Repeated calls with the
 * same `required=true` value intentionally do not refresh the heartbeat,
 * otherwise a stuck source could be kept alive by another task.
 *
 * Inputs: `source`, `required`.
 * Returns: None.
 */
void watchdog_set_required(watchdog_source_t source, bool required){
  if(!watchdog_source_valid_(source)){
    return;
  }

  const int64_t now_us = esp_timer_get_time();

  portENTER_CRITICAL(&s_wd_mux);
  const bool was_required = s_wd[source].required;
  s_wd[source].required = required;

  if(required && !was_required){
    s_wd[source].last_us = now_us;
    s_wd[source].seen_us = now_us;
    s_wd[source].missed_checks = 0u;
  } else if(!required){
    // Disabled sources are not expected to kick.  Align checker state so the
    // source starts cleanly if it becomes required again later.
    s_wd[source].seen_us = s_wd[source].last_us;
    s_wd[source].missed_checks = 0u;
  }
  portEXIT_CRITICAL(&s_wd_mux);
}

/**
 * Reports whether a persistent watchdog fault latch is present.
 *
 * Inputs: None.
 * Returns: `true` when the active watchdog fault flag is present.
 */
bool watchdog_persistent_fault_present(void){
  if(!s_prefs_open){
    return false;
  }

  // Ignore active latches from older diagnostic formats.  This prevents a
  // stale fault stored before a watchdog algorithm change from forcing the
  // device into FATAL WDG/CLR after the update.
  return watchdog_persistent_diag_valid_() &&
         s_prefs.getBool(WATCHDOG_PREFS_KEY, false);
}

/**
 * Clears the active watchdog fault latch while preserving the diagnostic
 * snapshot.
 *
 * Inputs: None.
 * Returns: None.
 */
void watchdog_persistent_fault_clear(void){
  if(!s_prefs_open){
    return;
  }

  // Clear only the active fault latch.  Keep the diagnostic snapshot so the
  // Web maintenance page can still show the watchdog cause after the operator
  // has acknowledged the message on the device UI.
  (void)s_prefs.remove(WATCHDOG_PREFS_KEY);
}

/**
 * Reads the last stored watchdog fault diagnostic from NVS.
 *
 * Inputs: `info` output pointer.
 * Returns: `true` when diagnostic data is available.
 */
bool watchdog_get_fault_info(watchdog_fault_info_t *info){
  if((info == nullptr) || !s_prefs_open){
    return false;
  }

  if(!watchdog_persistent_diag_valid_()){
    return false;
  }

  info->active = s_prefs.getBool(WATCHDOG_PREFS_KEY, false);
  info->source = (watchdog_source_t)s_prefs.getUChar("src", (uint8_t)WD_COUNT);
  info->age_ms = s_prefs.getUInt("age", 0u);
  info->ages_ms[WD_STATE] = s_prefs.getUInt("age_st", 0u);
  info->ages_ms[WD_SD] = s_prefs.getUInt("age_sd", 0u);
  info->ages_ms[WD_RECORD] = s_prefs.getUInt("age_rec", 0u);
  info->ages_ms[WD_WEB] = s_prefs.getUInt("age_web", 0u);
  info->recorder_state = s_prefs.getUInt("state", 0u);
  info->last_error = s_prefs.getInt("err", 0);
  info->web_active = s_prefs.getBool("web", false);
  info->usb_present = s_prefs.getBool("usb", false);
  info->sd_present = s_prefs.getBool("sd", false);
  info->heap = s_prefs.getUInt("heap", 0u);
  info->min_heap = s_prefs.getUInt("minheap", 0u);
  return true;
}

/**
 * Returns a short printable name for a watchdog source.
 *
 * Inputs: `source`.
 * Returns: Constant source name string.
 */
const char *watchdog_source_name(watchdog_source_t source){
  return watchdog_source_name_(source);
}

/**
 * Checks watchdog heartbeat progress and shuts down the recorder after storing
 * a persistent diagnostic if a required source has stopped progressing.
 *
 * The timeout decision is based on unchanged heartbeat tokens over consecutive
 * checker periods.  The reported ages are diagnostic only and are calculated
 * from the ESP timer so the Web page can show useful timing information.
 *
 * Inputs: None.
 * Returns: None.
 */
void watchdog_service_check(void){
  if(s_fault_handling){
    return;
  }

  bool timeout = false;
  watchdog_source_t failed_source = WD_COUNT;
  uint32_t failed_age_ms = 0u;
  uint32_t ages_ms[WD_COUNT] = {};
  const uint8_t missed_limit = watchdog_missed_limit_();

  portENTER_CRITICAL(&s_wd_mux);
  const int64_t now_us = esp_timer_get_time();

  for(uint8_t i = 0u; i < (uint8_t)WD_COUNT; ++i){
    // Reporting only: the decision below uses heartbeat progress, not age.
    ages_ms[i] = watchdog_elapsed_us_to_ms_(now_us - s_wd[i].last_us);

    if(!s_wd[i].required){
      s_wd[i].seen_us = s_wd[i].last_us;
      s_wd[i].missed_checks = 0u;
      continue;
    }

    if(s_wd[i].last_us != s_wd[i].seen_us){
      s_wd[i].seen_us = s_wd[i].last_us;
      s_wd[i].missed_checks = 0u;
    } else if(s_wd[i].missed_checks < 255u){
      s_wd[i].missed_checks++;
    }

    if((!timeout) && (s_wd[i].missed_checks >= missed_limit)){
      timeout = true;
      failed_source = (watchdog_source_t)i;
      failed_age_ms = ages_ms[i];
    }
  }
  portEXIT_CRITICAL(&s_wd_mux);

  if(!timeout){
    return;
  }

  s_fault_handling = true;

  const system_status_t st = state_task_get_status();
  watchdog_persistent_fault_set_(failed_source, failed_age_ms, ages_ms, &st);

  // A watchdog timeout means at least one monitored task failed to show
  // progress.  Do not attempt additional recovery work here; store the cause
  // and shut down deterministically.
  shutdown_device();
}
