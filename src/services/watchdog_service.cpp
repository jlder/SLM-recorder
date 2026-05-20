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
 * Stores the persistent watchdog fault flag in NVS.
 *
 * Inputs: None.
 * Returns: None.
 */
static void watchdog_persistent_fault_set_(void){
  if(!s_prefs_open){
    return;
  }

  (void)s_prefs.putBool(WATCHDOG_PREFS_KEY, true);
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
  s_fault_handling = false;
  portEXIT_CRITICAL(&s_wd_mux);

  s_prefs_open = s_prefs.begin(WATCHDOG_PREFS_NAMESPACE, false);
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
  s_wd[source].required = required;
  if(required){
    // Enabling a source starts a fresh timeout window.
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

  (void)s_prefs.remove(WATCHDOG_PREFS_KEY);
}

void watchdog_service_check(void){
  if(s_fault_handling){
    return;
  }

  const uint32_t now = watchdog_now_ms_();
  bool timeout = false;

  portENTER_CRITICAL(&s_wd_mux);
  for(uint8_t i = 0u; i < (uint8_t)WD_COUNT; ++i){
    if(s_wd[i].required && ((now - s_wd[i].last_ms) > (uint32_t)WATCHDOG_TIMEOUT_MS)){
      timeout = true;
      break;
    }
  }
  portEXIT_CRITICAL(&s_wd_mux);

  if(!timeout){
    return;
  }

  s_fault_handling = true;
  watchdog_persistent_fault_set_();

  const system_status_t st = state_task_get_status();
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
