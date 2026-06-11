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
#include <freertos/task.h>
#include <freertos/portmacro.h>

#include "config.h"
#include "src/models/system_status.h"
#include "src/tasks/state_task.h"
#include "src/tasks/sd_task.h"
#include "src/services/device_service.h"

typedef struct {
  uint32_t last_ms;
  bool required;
} watchdog_entry_t;

static Preferences s_prefs;
static bool s_prefs_open = false;
static bool s_fault_handling = false;
static watchdog_entry_t s_wd[WD_COUNT];

static portMUX_TYPE s_wd_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Returns the current FreeRTOS millisecond tick count.
 *
 * Inputs: None.
 * Returns: Milliseconds since scheduler start.
 */
static uint32_t watchdog_now_ms_(void){
  const TickType_t t = xTaskGetTickCount();
  return (uint32_t)(t * portTICK_PERIOD_MS);
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
 * Stores persistent watchdog fault details in NVS.
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
 * Prints a watchdog fault diagnostic snapshot to Serial.
 *
 * Inputs: `prefix`, `source`, `age_ms`, `ages_ms`, `st`.
 * Returns: None.
 */
static void watchdog_print_fault_(const char *prefix,
                                  watchdog_source_t source,
                                  uint32_t age_ms,
                                  const uint32_t ages_ms[WD_COUNT],
                                  const system_status_t *st){
  Serial.print(prefix != nullptr ? prefix : "WATCHDOG");
  Serial.print(": source=");
  Serial.print(watchdog_source_name_(source));
  Serial.print(" age_ms=");
  Serial.print((unsigned long)age_ms);

  if(ages_ms != nullptr){
    Serial.print(" ages_ms[state,sd,record,web]=[");
    Serial.print((unsigned long)ages_ms[WD_STATE]);
    Serial.print(",");
    Serial.print((unsigned long)ages_ms[WD_SD]);
    Serial.print(",");
    Serial.print((unsigned long)ages_ms[WD_RECORD]);
    Serial.print(",");
    Serial.print((unsigned long)ages_ms[WD_WEB]);
    Serial.print("]");
  }

  if(st != nullptr){
    Serial.print(" recorder_state=");
    Serial.print((unsigned long)st->state);
    Serial.print(" last_error=");
    Serial.print((long)st->last_error);
    Serial.print(" web=");
    Serial.print(st->wifi_active ? "on" : "off");
    Serial.print(" usb=");
    Serial.print((st->usb_present_valid && st->usb_present) ? "on" : "off");
    Serial.print(" sd=");
    Serial.print(st->sd_present ? "present" : "missing");
  }

  Serial.print(" heap=");
  Serial.print((unsigned long)ESP.getFreeHeap());
  Serial.print(" min_heap=");
  Serial.println((unsigned long)ESP.getMinFreeHeap());
  Serial.flush();
}

/**
 * Prints the persistent watchdog fault details from the previous shutdown.
 *
 * Inputs: None.
 * Returns: None.
 */
static void watchdog_print_persistent_fault_(void){
  if(!s_prefs_open || !s_prefs.getBool(WATCHDOG_PREFS_KEY, false)){
    return;
  }

  const watchdog_source_t source = (watchdog_source_t)s_prefs.getUChar("src", (uint8_t)WD_COUNT);
  uint32_t ages_ms[WD_COUNT] = {};
  ages_ms[WD_STATE] = s_prefs.getUInt("age_st", 0u);
  ages_ms[WD_SD] = s_prefs.getUInt("age_sd", 0u);
  ages_ms[WD_RECORD] = s_prefs.getUInt("age_rec", 0u);
  ages_ms[WD_WEB] = s_prefs.getUInt("age_web", 0u);

  Serial.print("WATCHDOG: previous fault source=");
  Serial.print(watchdog_source_name_(source));
  Serial.print(" age_ms=");
  Serial.print((unsigned long)s_prefs.getUInt("age", 0u));
  Serial.print(" ages_ms[state,sd,record,web]=[");
  Serial.print((unsigned long)ages_ms[WD_STATE]);
  Serial.print(",");
  Serial.print((unsigned long)ages_ms[WD_SD]);
  Serial.print(",");
  Serial.print((unsigned long)ages_ms[WD_RECORD]);
  Serial.print(",");
  Serial.print((unsigned long)ages_ms[WD_WEB]);
  Serial.print("] recorder_state=");
  Serial.print((unsigned long)s_prefs.getUInt("state", 0u));
  Serial.print(" last_error=");
  Serial.print((long)s_prefs.getInt("err", 0));
  Serial.print(" web=");
  Serial.print(s_prefs.getBool("web", false) ? "on" : "off");
  Serial.print(" usb=");
  Serial.print(s_prefs.getBool("usb", false) ? "on" : "off");
  Serial.print(" sd=");
  Serial.print(s_prefs.getBool("sd", false) ? "present" : "missing");
  Serial.print(" heap=");
  Serial.print((unsigned long)s_prefs.getUInt("heap", 0u));
  Serial.print(" min_heap=");
  Serial.println((unsigned long)s_prefs.getUInt("minheap", 0u));
  Serial.flush();
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

void watchdog_service_init(void){
  const uint32_t now = watchdog_now_ms_();

  portENTER_CRITICAL(&s_wd_mux);
  for(uint8_t i = 0u; i < (uint8_t)WD_COUNT; ++i){
    s_wd[i].last_ms = now;
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
  watchdog_print_persistent_fault_();
}

void watchdog_kick(watchdog_source_t source){
  if(!watchdog_source_valid_(source)){
    return;
  }

  const uint32_t now = watchdog_now_ms_();

  portENTER_CRITICAL(&s_wd_mux);
  s_wd[source].last_ms = now;
  portEXIT_CRITICAL(&s_wd_mux);
}

void watchdog_set_required(watchdog_source_t source, bool required){
  if(!watchdog_source_valid_(source)){
    return;
  }

  const uint32_t now = watchdog_now_ms_();

  portENTER_CRITICAL(&s_wd_mux);
  const bool was_required = s_wd[source].required;
  s_wd[source].required = required;
  if(required && !was_required){
    // Enabling a source starts a fresh timeout window.  Repeated calls with
    // required=true must not refresh the heartbeat, otherwise the monitored
    // source could never time out.
    s_wd[source].last_ms = now;
  }
  portEXIT_CRITICAL(&s_wd_mux);
}

bool watchdog_persistent_fault_present(void){
  if(!s_prefs_open){
    return false;
  }

  return s_prefs.getBool(WATCHDOG_PREFS_KEY, false);
}

void watchdog_persistent_fault_clear(void){
  if(!s_prefs_open){
    return;
  }

  // Clear only the active fault latch.  Keep the diagnostic snapshot so the
  // Web maintenance page can still show the watchdog cause after the operator
  // has acknowledged the message on the device UI.
  (void)s_prefs.remove(WATCHDOG_PREFS_KEY);
}

bool watchdog_get_fault_info(watchdog_fault_info_t *info){
  if((info == nullptr) || !s_prefs_open){
    return false;
  }

  if(!s_prefs.getBool("valid", false)){
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

const char *watchdog_source_name(watchdog_source_t source){
  return watchdog_source_name_(source);
}

void watchdog_service_check(void){
  if(s_fault_handling){
    return;
  }

  const uint32_t now = watchdog_now_ms_();
  bool timeout = false;
  watchdog_source_t failed_source = WD_COUNT;
  uint32_t failed_age_ms = 0u;
  uint32_t ages_ms[WD_COUNT] = {};

  portENTER_CRITICAL(&s_wd_mux);
  for(uint8_t i = 0u; i < (uint8_t)WD_COUNT; ++i){
    ages_ms[i] = now - s_wd[i].last_ms;
    if((!timeout) && s_wd[i].required && (ages_ms[i] > (uint32_t)WATCHDOG_TIMEOUT_MS)){
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
  watchdog_print_fault_("WATCHDOG: timeout", failed_source, failed_age_ms, ages_ms, &st);
  const bool recording_related =
      (st.state == ST_RECORDING) ||
      (st.state == ST_STARTING) ||
      (st.state == ST_STOPPING);

  if(recording_related){
    sd_request_close();

    const uint32_t wait_start = watchdog_now_ms_();
    while(!sd_is_closed() &&
          ((watchdog_now_ms_() - wait_start) <= (uint32_t)WATCHDOG_TIMEOUT_MS)){
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }

  shutdown_device();
}
