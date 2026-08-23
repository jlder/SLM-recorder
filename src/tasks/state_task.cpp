// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file state_task.cpp
 * @brief Recorder state machine task.
 *
 * @details
 * This module owns the recorder high-level state machine.
 *
 * The implementation follows a strict, readable structure for each state:
 *   A) One-time actions   (executed once on entry)
 *   B) Recurring actions  (executed each tick while the state is active)
 *   C) State changes      (exclusive conditions that transition to a new state)
 *
 * Design notes:
 * - Only this module shall change recorder_state_t.
 * - first_pass shall become true only when a state transition occurs.
 * - Transient states (BOOT/STARTING/STOPPING/OFF) shall use a single entry timestamp.
 * - Button events used by the state machine shall be latched until explicitly cleared.
 * - Other tasks/drivers shall provide status and accept requests; they shall not change state.
 */

#include "src/tasks/state_task.h"
#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>
#include <string.h>
#include "src/services/task_helpers.h"

#include "src/global.h"
#include "src/services/error_manager.h"
#include "src/services/button_hold_helpers.h"
#include "src/services/device_service.h"
#include "src/services/ring_buffer.h"
#include "src/services/timebase.h"
#include "src/services/record_format.h"
#include "src/services/settings_store.h" // read-only settings snapshot
#include "src/services/settings_store_write.h" // State-task-only settings writes
#include "src/services/datetime_service.h"
#include "src/services/calibration_service.h"
#include "src/services/watchdog_service.h"
#include "src/services/audio_alert_service.h"
#include "src/services/automation_service.h"

#include "src/services/touch_service.h"
#include "src/tasks/sd_task.h"
#include "src/tasks/web_task.h"
extern bool settings_storage_ok;

// =============================================================================
// Local types
// =============================================================================

// =============================================================================
// Internal state
// =============================================================================

static TaskHandle_t s_task = nullptr;

// State-task private working copy. Only state_task_main() and helpers in this
// file modify it directly. Other tasks shall read only the published snapshot
// through state_task_get_status().
static system_status_t s_st = {};

// Cross-core published status snapshot. UI, Web, and SD-side consumers may run
// on a different ESP32 core, so copying the multi-field system_status_t must be
// protected against torn reads while the state task publishes a new snapshot.
static system_status_t s_st_pub = {};
static portMUX_TYPE s_st_pub_mux = portMUX_INITIALIZER_UNLOCKED;

static bool s_first_pass = true;
static uint32_t s_entry_ms = 0u;
static uint32_t s_state_tick = 0u;
// Set when power-long is requested during recording.  The recorder first
// closes the SD file through ST_STOPPING, then continues to ST_OFF.
static bool s_shutdown_after_stop_requested = false;
// True only for a recording whose start was requested by AUTO RECORDING.
// AUTO DELETE is never applied to a manually started recording.
static bool s_recording_automatic = false;
// Persistent watchdog fault acknowledgement latch.  When set, startup
// waits for Power/Clear before normal BOOT checks continue.
static bool s_watchdog_ack_pending = false;
// State task is the sole runtime authority for recorder settings changes.

// UI request latches (simple, no queues).
// UI tasks may run on another core, therefore the two command flags are
// protected by a small spinlock.  The State task consumes the flags and still
// owns all recorder-state transitions.
static portMUX_TYPE s_ui_cmd_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_ui_record_start_requested = false;
static bool s_ui_record_stop_requested = false;

typedef struct {
  bool wifi_toggle;
  bool auto_recording_toggle;
  bool auto_wifi_toggle;
  bool auto_delete_toggle;
  bool language_toggle;
  bool set_date;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  bool set_time;
  uint8_t hour;
  uint8_t minute;
  bool set_registration;
  char registration[SETTINGS_REGISTRATION_LEN];
} ui_control_requests_t;

static ui_control_requests_t s_ui_control_requests = {};

static bool automation_diag_force_all_on_(void);
static bool effective_auto_recording_(bool settings_loaded, const settings_t &settings);
static bool effective_auto_wifi_(bool settings_loaded, const settings_t &settings);
static bool effective_auto_delete_(bool settings_loaded, const settings_t &settings);


#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
static_assert(AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG >
                  (2 * AUTOMATION_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG),
              "diagnostic axis bands must not overlap");
static_assert((AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG +
               AUTOMATION_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG) <= 32767,
              "diagnostic axis encoding must fit int16");

// Diagnostic wire code 13 is deliberately the all-zero-offset ternary value
// (1,1,1) and therefore leaves the recorded acceleration sample unchanged.
enum automation_diag_event_t : uint8_t {
  DIAG_AUTO_START_MOTION_POLICY = 0u,
  DIAG_AUTO_START_ATTITUDE_POLICY = 1u,
  DIAG_MOTION_START_ON = 2u,
  DIAG_MOTION_START_OFF = 3u,
  DIAG_ATTITUDE_START_ON = 4u,
  DIAG_ATTITUDE_START_OFF = 5u,
  DIAG_MOTION_STOP_ON = 6u,
  DIAG_MOTION_STOP_OFF = 7u,
  DIAG_HIRMS_ABOVE = 8u,
  DIAG_HIRMS_BELOW = 9u,
  DIAG_PRIMARY_CONFIRMING = 10u,
  DIAG_POSSIBLE_FLIGHT = 11u,
  DIAG_FLIGHT_SEEN_PRIMARY = 12u,
  DIAG_NONE = 13u,
  DIAG_FLIGHT_SEEN_SECONDARY = 14u,
  DIAG_HIRMS_EVENT_START = 15u,
  DIAG_HIRMS_EVENT_3S = 16u,
  DIAG_HIRMS_EVENT_SAW_FLIGHT_FG = 17u,
  DIAG_HIRMS_EVENT_END_VALID = 18u,
  DIAG_HIRMS_EVENT_END_INVALID = 19u,
  DIAG_LANDING_EVENT_EXPIRED = 20u,
  DIAG_FG_LOW_START = 21u,
  DIAG_FG_LOW_RESET = 22u,
  DIAG_GROUND_CANDIDATE = 23u,
  DIAG_GROUND_CANCELLED = 24u,
  DIAG_FLIGHT_END_CONFIRMED = 25u,
  DIAG_EXTENDED = 26u
};

enum automation_diag_extended_event_t : uint8_t {
  DIAG_EXT_QUEUE_OVERFLOW = 0u,
  DIAG_EXT_AUTO_WIFI_SELECTED_ON = 1u,
  DIAG_EXT_AUTO_WIFI_SELECTED_OFF = 2u,
  DIAG_EXT_WIFI_REQUESTED_ON = 3u,
  DIAG_EXT_WIFI_REQUESTED_OFF = 4u,
  DIAG_EXT_WIFI_AP_STARTED = 5u,
  DIAG_EXT_WIFI_AP_STOPPED = 6u,
  DIAG_EXT_AUTO_RECORD_SELECTED_ON = 7u,
  DIAG_EXT_AUTO_RECORD_SELECTED_OFF = 8u,
  DIAG_EXT_AUTO_DELETE_SELECTED_ON = 9u,
  DIAG_EXT_AUTO_DELETE_SELECTED_OFF = 10u,
  DIAG_EXT_USB_PRESENT = 11u,
  DIAG_EXT_USB_ABSENT = 12u,
  DIAG_EXT_STATE_READY = 13u,
  DIAG_EXT_STATE_STARTING = 14u,
  DIAG_EXT_STATE_RECORDING = 15u,
  DIAG_EXT_STATE_STOPPING = 16u,
  DIAG_EXT_STATE_ERROR = 17u,
  DIAG_EXT_STATE_OFF = 18u,
  DIAG_EXT_MANUAL_RECORD_START = 19u,
  DIAG_EXT_GROUND_10S = 20u,
  DIAG_EXT_GROUND_25S = 21u,
  DIAG_EXT_GROUND_40S = 22u,
  DIAG_EXT_VIRTUAL_AUTO_SESSION_START = 23u,
  DIAG_EXT_VIRTUAL_AUTO_STOP_NO_FLIGHT = 24u,
  DIAG_EXT_VIRTUAL_AUTO_STOP_FLIGHT_END = 25u,
  DIAG_EXTENDED_2 = 26u
};

enum automation_diag_extended2_event_t : uint8_t {
  DIAG_EXT2_OBSERVE_ONLY_ACTIVE = 0u,
  DIAG_EXT2_WOULD_AUTO_WIFI_ON = 1u,
  DIAG_EXT2_WOULD_AUTO_WIFI_OFF_MOTION = 2u,
  DIAG_EXT2_WOULD_AUTO_WIFI_OFF_RECORDING = 3u,
  DIAG_EXT2_WOULD_AUTO_WIFI_OFF_SELECTED = 4u,
  DIAG_EXT2_WOULD_AUTO_DELETE = 5u,
  DIAG_EXT2_VIRTUAL_READY = 6u,
  DIAG_EXT2_VIRTUAL_RECORDING = 7u,
  DIAG_EXT2_AUTO_WIFI_QUIET_5S = 8u
};

static uint8_t s_diag_queue[AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH] = {};
static uint32_t s_diag_queue_head = 0u;
static uint32_t s_diag_queue_tail = 0u;
static uint32_t s_diag_queue_count = 0u;
static uint8_t s_diag_next_wire_code = DIAG_NONE;
static int16_t s_diag_next_axis_offset_mg[3] = {0, 0, 0};
static bool s_diag_prev_valid = false;
static automation_status_t s_diag_prev_status = {};
static automation_debug_status_t s_diag_prev_debug = {};
static bool s_diag_start_motion_policy = false;
static bool s_diag_start_attitude_policy = false;
static bool s_diag_system_prev_valid = false;
static recorder_state_t s_diag_prev_recorder_state = ST_BOOT;
static bool s_diag_prev_auto_record = false;
static bool s_diag_prev_auto_wifi = false;
static bool s_diag_prev_auto_delete = false;
static bool s_diag_prev_wifi_requested = false;
static bool s_diag_prev_wifi_started = false;
static bool s_diag_prev_usb_valid = false;
static bool s_diag_prev_usb_present = false;
static bool s_diag_ground_10s_emitted = false;
static bool s_diag_ground_25s_emitted = false;
static bool s_diag_ground_40s_emitted = false;
static bool s_diag_virtual_auto_mode = false;
static bool s_diag_virtual_session_active = false;
static bool s_diag_observe_only_recording = false;
static bool s_diag_virtual_wifi_on = false;
static bool s_diag_virtual_wifi_wait_quiet = false;
static uint32_t s_diag_virtual_wifi_quiet_count = 0u;

static void diagnostic_queue_reset_(void){
  s_diag_queue_head = 0u;
  s_diag_queue_tail = 0u;
  s_diag_queue_count = 0u;
  s_diag_next_wire_code = DIAG_NONE;
  s_diag_next_axis_offset_mg[0] = 0;
  s_diag_next_axis_offset_mg[1] = 0;
  s_diag_next_axis_offset_mg[2] = 0;
  s_diag_prev_valid = false;
  memset(&s_diag_prev_status, 0, sizeof(s_diag_prev_status));
  memset(&s_diag_prev_debug, 0, sizeof(s_diag_prev_debug));
  s_diag_ground_10s_emitted = false;
  s_diag_ground_25s_emitted = false;
  s_diag_ground_40s_emitted = false;
}

static void diagnostic_mark_queue_overflow_(void){
  if(s_diag_queue_count >= 2u){
    const uint32_t newest =
        (s_diag_queue_tail + (uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH - 1u) %
        (uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH;
    const uint32_t previous =
        (s_diag_queue_tail + (uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH - 2u) %
        (uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH;
    s_diag_queue[previous] = DIAG_EXTENDED;
    s_diag_queue[newest] = DIAG_EXT_QUEUE_OVERFLOW;
  }
}

static void diagnostic_enqueue_(uint8_t event_code){
  if(event_code == DIAG_NONE){
    return;
  }
  if(s_diag_queue_count >= (uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH){
    diagnostic_mark_queue_overflow_();
    return;
  }
  s_diag_queue[s_diag_queue_tail] = event_code;
  s_diag_queue_tail =
      (s_diag_queue_tail + 1u) % (uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH;
  ++s_diag_queue_count;
}

static void diagnostic_enqueue_extended_(uint8_t event_code){
  if(s_diag_queue_count > ((uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH - 2u)){
    diagnostic_mark_queue_overflow_();
    return;
  }
  diagnostic_enqueue_(DIAG_EXTENDED);
  diagnostic_enqueue_(event_code);
}

static void diagnostic_enqueue_extended2_(uint8_t event_code){
  if(s_diag_queue_count > ((uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH - 3u)){
    diagnostic_mark_queue_overflow_();
    return;
  }
  diagnostic_enqueue_(DIAG_EXTENDED);
  diagnostic_enqueue_(DIAG_EXTENDED_2);
  diagnostic_enqueue_(event_code);
}

static uint8_t diagnostic_dequeue_(void){
  if(s_diag_queue_count == 0u){
    return DIAG_NONE;
  }
  const uint8_t out = s_diag_queue[s_diag_queue_head];
  s_diag_queue_head =
      (s_diag_queue_head + 1u) % (uint32_t)AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH;
  --s_diag_queue_count;
  return out;
}

static void diagnostic_prepare_overlay_(uint8_t wire_code){
  const uint8_t tx = (uint8_t)(wire_code % 3u);
  const uint8_t ty = (uint8_t)((wire_code / 3u) % 3u);
  const uint8_t tz = (uint8_t)((wire_code / 9u) % 3u);
  s_diag_next_axis_offset_mg[0] =
      (int16_t)(((int32_t)tx - 1) * (int32_t)AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG);
  s_diag_next_axis_offset_mg[1] =
      (int16_t)(((int32_t)ty - 1) * (int32_t)AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG);
  s_diag_next_axis_offset_mg[2] =
      (int16_t)(((int32_t)tz - 1) * (int32_t)AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG);
}

static void diagnostic_apply_prepared_overlay_(accel_sample_t *sample){
  if(sample == nullptr){
    return;
  }
  sample->ax = (int16_t)((int32_t)sample->ax + (int32_t)s_diag_next_axis_offset_mg[0]);
  sample->ay = (int16_t)((int32_t)sample->ay + (int32_t)s_diag_next_axis_offset_mg[1]);
  sample->az = (int16_t)((int32_t)sample->az + (int32_t)s_diag_next_axis_offset_mg[2]);
}

static void diagnostic_begin_recording_(void){
  s_diag_prev_status = automation_service_status();
  s_diag_prev_debug = automation_service_debug_status();
  s_diag_prev_valid = true;
  s_diag_ground_10s_emitted = false;
  s_diag_ground_25s_emitted = false;
  s_diag_ground_40s_emitted = false;
  s_diag_virtual_wifi_on = false;
  s_diag_virtual_wifi_wait_quiet = false;
  s_diag_virtual_wifi_quiet_count = 0u;

  if(s_diag_observe_only_recording){
    diagnostic_enqueue_extended2_(DIAG_EXT2_OBSERVE_ONLY_ACTIVE);
    diagnostic_enqueue_extended2_(DIAG_EXT2_VIRTUAL_READY);
  }

  if(s_diag_start_motion_policy){
    diagnostic_enqueue_(DIAG_AUTO_START_MOTION_POLICY);
  }
  if(s_diag_start_attitude_policy){
    diagnostic_enqueue_(DIAG_AUTO_START_ATTITUDE_POLICY);
  }
  s_diag_start_motion_policy = false;
  s_diag_start_attitude_policy = false;

  if(s_diag_next_wire_code == DIAG_NONE){
    s_diag_next_wire_code = diagnostic_dequeue_();
    diagnostic_prepare_overlay_(s_diag_next_wire_code);
  }
}

static void diagnostic_update_after_cycle_(void){
  // Event extraction deliberately runs at the end of the State-task cycle,
  // after the current recording sample has already been pushed to the ring.
  // The selected wire code is therefore carried by a later 20 Hz sample.

  settings_t settings = {};
  const bool settings_loaded = settings_get(&settings);
  const bool auto_record = effective_auto_recording_(settings_loaded, settings);
  const bool auto_wifi = effective_auto_wifi_(settings_loaded, settings);
  const bool auto_delete = effective_auto_delete_(settings_loaded, settings);
  const bool wifi_requested = web_task_is_enabled();
  const bool wifi_started = web_task_is_started();

  if(!s_diag_system_prev_valid){
    s_diag_prev_recorder_state = s_st.state;
    s_diag_prev_auto_record = auto_record;
    s_diag_prev_auto_wifi = auto_wifi;
    s_diag_prev_auto_delete = auto_delete;
    s_diag_prev_wifi_requested = wifi_requested;
    s_diag_prev_wifi_started = wifi_started;
    s_diag_prev_usb_valid = s_st.usb_present_valid;
    s_diag_prev_usb_present = s_st.usb_present;
    s_diag_system_prev_valid = true;

    if(auto_record){ diagnostic_enqueue_extended_(DIAG_EXT_AUTO_RECORD_SELECTED_ON); }
    if(auto_wifi){ diagnostic_enqueue_extended_(DIAG_EXT_AUTO_WIFI_SELECTED_ON); }
    if(auto_delete){ diagnostic_enqueue_extended_(DIAG_EXT_AUTO_DELETE_SELECTED_ON); }
    if(wifi_requested){ diagnostic_enqueue_extended_(DIAG_EXT_WIFI_REQUESTED_ON); }
    if(wifi_started){ diagnostic_enqueue_extended_(DIAG_EXT_WIFI_AP_STARTED); }
    if(s_st.usb_present_valid){
      diagnostic_enqueue_extended_(s_st.usb_present ? DIAG_EXT_USB_PRESENT
                                                    : DIAG_EXT_USB_ABSENT);
    }
  } else {
    if(s_st.state != s_diag_prev_recorder_state){
      uint8_t ext = DIAG_EXT_STATE_READY;
      switch(s_st.state){
        case ST_READY: ext = DIAG_EXT_STATE_READY; break;
        case ST_STARTING: ext = DIAG_EXT_STATE_STARTING; break;
        case ST_RECORDING: ext = DIAG_EXT_STATE_RECORDING; break;
        case ST_STOPPING: ext = DIAG_EXT_STATE_STOPPING; break;
        case ST_ERROR: ext = DIAG_EXT_STATE_ERROR; break;
        case ST_OFF: ext = DIAG_EXT_STATE_OFF; break;
        default: break;
      }
      diagnostic_enqueue_extended_(ext);
      s_diag_prev_recorder_state = s_st.state;
    }
    if(auto_record != s_diag_prev_auto_record){
      diagnostic_enqueue_extended_(auto_record ? DIAG_EXT_AUTO_RECORD_SELECTED_ON
                                               : DIAG_EXT_AUTO_RECORD_SELECTED_OFF);
      s_diag_prev_auto_record = auto_record;
    }
    if(auto_wifi != s_diag_prev_auto_wifi){
      diagnostic_enqueue_extended_(auto_wifi ? DIAG_EXT_AUTO_WIFI_SELECTED_ON
                                             : DIAG_EXT_AUTO_WIFI_SELECTED_OFF);
      s_diag_prev_auto_wifi = auto_wifi;
    }
    if(auto_delete != s_diag_prev_auto_delete){
      diagnostic_enqueue_extended_(auto_delete ? DIAG_EXT_AUTO_DELETE_SELECTED_ON
                                               : DIAG_EXT_AUTO_DELETE_SELECTED_OFF);
      s_diag_prev_auto_delete = auto_delete;
    }
    if(wifi_requested != s_diag_prev_wifi_requested){
      diagnostic_enqueue_extended_(wifi_requested ? DIAG_EXT_WIFI_REQUESTED_ON
                                                  : DIAG_EXT_WIFI_REQUESTED_OFF);
      s_diag_prev_wifi_requested = wifi_requested;
    }
    if(wifi_started != s_diag_prev_wifi_started){
      diagnostic_enqueue_extended_(wifi_started ? DIAG_EXT_WIFI_AP_STARTED
                                                : DIAG_EXT_WIFI_AP_STOPPED);
      s_diag_prev_wifi_started = wifi_started;
    }
    if(s_st.usb_present_valid &&
       ((!s_diag_prev_usb_valid) || (s_st.usb_present != s_diag_prev_usb_present))){
      diagnostic_enqueue_extended_(s_st.usb_present ? DIAG_EXT_USB_PRESENT
                                                    : DIAG_EXT_USB_ABSENT);
      s_diag_prev_usb_present = s_st.usb_present;
    }
    s_diag_prev_usb_valid = s_st.usb_present_valid;
  }

  if(s_st.state != ST_RECORDING){
    if(s_diag_next_wire_code == DIAG_NONE){
      s_diag_next_wire_code = diagnostic_dequeue_();
      diagnostic_prepare_overlay_(s_diag_next_wire_code);
    }
    return;
  }

  const automation_status_t status = automation_service_status();
  const automation_debug_status_t debug = automation_service_debug_status();

  if(!s_diag_prev_valid){
    s_diag_prev_status = status;
    s_diag_prev_debug = debug;
    s_diag_prev_valid = true;
  } else {
    if(status.motion_start_confirmed != s_diag_prev_status.motion_start_confirmed){
      diagnostic_enqueue_(status.motion_start_confirmed ? DIAG_MOTION_START_ON
                                                        : DIAG_MOTION_START_OFF);
    }
    if(status.attitude_start_confirmed != s_diag_prev_status.attitude_start_confirmed){
      diagnostic_enqueue_(status.attitude_start_confirmed ? DIAG_ATTITUDE_START_ON
                                                          : DIAG_ATTITUDE_START_OFF);
    }
    if(status.motion_stop_confirmed != s_diag_prev_status.motion_stop_confirmed){
      diagnostic_enqueue_(status.motion_stop_confirmed ? DIAG_MOTION_STOP_ON
                                                       : DIAG_MOTION_STOP_OFF);
    }
    if(debug.hirms_above != s_diag_prev_debug.hirms_above){
      diagnostic_enqueue_(debug.hirms_above ? DIAG_HIRMS_ABOVE : DIAG_HIRMS_BELOW);
    }
    if(debug.primary_confirming && !s_diag_prev_debug.primary_confirming){
      diagnostic_enqueue_(DIAG_PRIMARY_CONFIRMING);
    }
    if(debug.possible_flight && !s_diag_prev_debug.possible_flight){
      diagnostic_enqueue_(DIAG_POSSIBLE_FLIGHT);
    }
    if(status.flight_seen && !s_diag_prev_status.flight_seen){
      diagnostic_enqueue_(debug.primary_confirmed ? DIAG_FLIGHT_SEEN_PRIMARY
                                                   : DIAG_FLIGHT_SEEN_SECONDARY);
    }
    if(debug.hirms_event_active && !s_diag_prev_debug.hirms_event_active){
      diagnostic_enqueue_(DIAG_HIRMS_EVENT_START);
    }
    if(debug.hirms_event_duration_ok && !s_diag_prev_debug.hirms_event_duration_ok){
      diagnostic_enqueue_(DIAG_HIRMS_EVENT_3S);
    }
    if(debug.hirms_event_saw_flight_fg && !s_diag_prev_debug.hirms_event_saw_flight_fg){
      diagnostic_enqueue_(DIAG_HIRMS_EVENT_SAW_FLIGHT_FG);
    }
    if(s_diag_prev_debug.hirms_event_active && !debug.hirms_event_active){
      const bool valid_event_started =
          debug.valid_landing_event && !s_diag_prev_debug.valid_landing_event;
      diagnostic_enqueue_(valid_event_started ? DIAG_HIRMS_EVENT_END_VALID
                                              : DIAG_HIRMS_EVENT_END_INVALID);
    }
    if(s_diag_prev_debug.valid_landing_event && !debug.valid_landing_event &&
       !debug.ground_candidate){
      diagnostic_enqueue_(DIAG_LANDING_EVENT_EXPIRED);
    }
    if(debug.fg_low_confirming && !s_diag_prev_debug.fg_low_confirming){
      diagnostic_enqueue_(DIAG_FG_LOW_START);
    }
    if(s_diag_prev_debug.fg_low_confirming && !debug.fg_low_confirming &&
       !debug.ground_candidate){
      diagnostic_enqueue_(DIAG_FG_LOW_RESET);
    }
    if(debug.ground_candidate && !s_diag_prev_debug.ground_candidate){
      diagnostic_enqueue_(DIAG_GROUND_CANDIDATE);
    }
    if(s_diag_prev_debug.ground_candidate && !debug.ground_candidate &&
       !status.flight_end_confirmed){
      diagnostic_enqueue_(DIAG_GROUND_CANCELLED);
    }
    if(status.flight_end_confirmed && !s_diag_prev_status.flight_end_confirmed){
      diagnostic_enqueue_(DIAG_FLIGHT_END_CONFIRMED);
    }

    if(debug.ground_candidate){
      const uint32_t n10 = (uint32_t)(10.0f * 20.0f + 0.5f);
      const uint32_t n25 = (uint32_t)(25.0f * 20.0f + 0.5f);
      const uint32_t n40 = (uint32_t)(40.0f * 20.0f + 0.5f);
      if(!s_diag_ground_10s_emitted && debug.ground_count_samples >= n10){
        diagnostic_enqueue_extended_(DIAG_EXT_GROUND_10S);
        s_diag_ground_10s_emitted = true;
      }
      if(!s_diag_ground_25s_emitted && debug.ground_count_samples >= n25){
        diagnostic_enqueue_extended_(DIAG_EXT_GROUND_25S);
        s_diag_ground_25s_emitted = true;
      }
      if(!s_diag_ground_40s_emitted && debug.ground_count_samples >= n40){
        diagnostic_enqueue_extended_(DIAG_EXT_GROUND_40S);
        s_diag_ground_40s_emitted = true;
      }
    } else {
      s_diag_ground_10s_emitted = false;
      s_diag_ground_25s_emitted = false;
      s_diag_ground_40s_emitted = false;
    }

    bool virtual_detector_reset = false;
    if(s_diag_virtual_auto_mode){
      if(!s_diag_virtual_session_active){
        const bool virtual_start =
            status.motion_start_confirmed || status.attitude_start_confirmed;
        if(virtual_start){
          if(status.motion_start_confirmed){
            diagnostic_enqueue_(DIAG_AUTO_START_MOTION_POLICY);
          }
          if(status.attitude_start_confirmed){
            diagnostic_enqueue_(DIAG_AUTO_START_ATTITUDE_POLICY);
          }
          diagnostic_enqueue_extended_(DIAG_EXT_VIRTUAL_AUTO_SESSION_START);
          diagnostic_enqueue_extended2_(DIAG_EXT2_VIRTUAL_RECORDING);
          s_diag_virtual_session_active = true;
          automation_service_begin_recording();
          automation_service_reset_start_confirmation();
          virtual_detector_reset = true;
        }
      } else if(!status.flight_seen && status.motion_stop_confirmed){
        diagnostic_enqueue_extended_(DIAG_EXT_VIRTUAL_AUTO_STOP_NO_FLIGHT);
        if(auto_delete){
          diagnostic_enqueue_extended2_(DIAG_EXT2_WOULD_AUTO_DELETE);
        }
        diagnostic_enqueue_extended2_(DIAG_EXT2_VIRTUAL_READY);
        s_diag_virtual_session_active = false;
        automation_service_begin_recording();
        automation_service_reset_start_confirmation();
        virtual_detector_reset = true;
      } else if(status.flight_seen && status.flight_end_confirmed){
        diagnostic_enqueue_extended_(DIAG_EXT_VIRTUAL_AUTO_STOP_FLIGHT_END);
        diagnostic_enqueue_extended2_(DIAG_EXT2_VIRTUAL_READY);
        s_diag_virtual_session_active = false;
        automation_service_begin_recording();
        automation_service_reset_start_confirmation();
        virtual_detector_reset = true;
      }
    }

    // Observe-only AUTO WIFI policy trace. This simulates the intended virtual
    // READY/RECORDING behavior without touching the actual Web/AP state. AUTO
    // WIFI is initially ON in a quiet virtual READY state, turns OFF for motion
    // or a virtual recording, and returns ON after 5 s of quiet.
    if(s_diag_observe_only_recording){
      const uint32_t quiet_needed =
          (uint32_t)(AUTOMATION_DIAGNOSTIC_AUTO_WIFI_QUIET_S * 20.0f + 0.5f);

      if(!auto_wifi){
        if(s_diag_virtual_wifi_on){
          diagnostic_enqueue_extended2_(DIAG_EXT2_WOULD_AUTO_WIFI_OFF_SELECTED);
        }
        s_diag_virtual_wifi_on = false;
        s_diag_virtual_wifi_wait_quiet = false;
        s_diag_virtual_wifi_quiet_count = 0u;
      } else if(s_diag_virtual_session_active){
        if(s_diag_virtual_wifi_on){
          diagnostic_enqueue_extended2_(DIAG_EXT2_WOULD_AUTO_WIFI_OFF_RECORDING);
        }
        s_diag_virtual_wifi_on = false;
        s_diag_virtual_wifi_wait_quiet = true;
        s_diag_virtual_wifi_quiet_count = 0u;
      } else if(status.motion_start_confirmed){
        if(s_diag_virtual_wifi_on){
          diagnostic_enqueue_extended2_(DIAG_EXT2_WOULD_AUTO_WIFI_OFF_MOTION);
        }
        s_diag_virtual_wifi_on = false;
        s_diag_virtual_wifi_wait_quiet = true;
        s_diag_virtual_wifi_quiet_count = 0u;
      } else if(s_diag_virtual_wifi_wait_quiet){
        if(status.motion_rms_g < (float)AUTO_RECORD_MOTION_THRESHOLD_G){
          if(s_diag_virtual_wifi_quiet_count < quiet_needed){
            ++s_diag_virtual_wifi_quiet_count;
          }
          if(s_diag_virtual_wifi_quiet_count >= quiet_needed){
            diagnostic_enqueue_extended2_(DIAG_EXT2_AUTO_WIFI_QUIET_5S);
            diagnostic_enqueue_extended2_(DIAG_EXT2_WOULD_AUTO_WIFI_ON);
            s_diag_virtual_wifi_on = true;
            s_diag_virtual_wifi_wait_quiet = false;
          }
        } else {
          s_diag_virtual_wifi_quiet_count = 0u;
        }
      } else if(!s_diag_virtual_wifi_on){
        diagnostic_enqueue_extended2_(DIAG_EXT2_WOULD_AUTO_WIFI_ON);
        s_diag_virtual_wifi_on = true;
      }
    }

    if(virtual_detector_reset){
      s_diag_prev_status = automation_service_status();
      s_diag_prev_debug = automation_service_debug_status();
      s_diag_ground_10s_emitted = false;
      s_diag_ground_25s_emitted = false;
      s_diag_ground_40s_emitted = false;
    } else {
      s_diag_prev_status = status;
      s_diag_prev_debug = debug;
    }
  }

  // A wire code stays pending until a recording sample actually consumes it.
  // If the previous code was consumed in acceleration_service_(), load the next
  // queued event now for the following sample.
  if(s_diag_next_wire_code == DIAG_NONE){
    s_diag_next_wire_code = diagnostic_dequeue_();
    diagnostic_prepare_overlay_(s_diag_next_wire_code);
  }
}
#endif

// =============================================================================
// Published status snapshot helpers
// =============================================================================

/**
 * Publishes the current state-task-owned working status as a coherent snapshot
 * for other tasks.
 *
 * The critical section deliberately copies only the status structure. It shall
 * not call services or drivers while the spinlock is held.
 */
static void publish_status_snapshot_(void){
  portENTER_CRITICAL(&s_st_pub_mux);
  s_st_pub = s_st;
  portEXIT_CRITICAL(&s_st_pub_mux);
}

/**
 * Copies the latest published status snapshot without exposing readers to a
 * partially updated multi-field structure.
 *
 * Returns: Coherent published system status snapshot.
 */
static system_status_t copy_status_snapshot_(void){
  system_status_t out = {};

  portENTER_CRITICAL(&s_st_pub_mux);
  out = s_st_pub;
  portEXIT_CRITICAL(&s_st_pub_mux);

  return out;
}

// =============================================================================
// UI command helpers
// =============================================================================

/**
 * Consume a pending UI start-recording command.
 *
 * The latch is cleared even if the caller later decides that recording start
 * is not allowed.  This makes a rejected UI request one-shot and prevents it
 * from being applied later after conditions change.
 *
 * Inputs: None.
 * Returns: `true` when the UI requested recording start.
 */
static bool ui_take_record_start_request_(void){
  bool requested = false;

  portENTER_CRITICAL(&s_ui_cmd_mux);
  requested = s_ui_record_start_requested;
  s_ui_record_start_requested = false;
  portEXIT_CRITICAL(&s_ui_cmd_mux);

  return requested;
}

/**
 * Consume a pending UI stop-recording command.
 *
 * The latch is cleared even if the caller later decides that recording stop is
 * not applicable.  This keeps UI commands simple one-shot requests.
 *
 * Inputs: None.
 * Returns: `true` when the UI requested recording stop.
 */
static bool ui_take_record_stop_request_(void){
  bool requested = false;

  portENTER_CRITICAL(&s_ui_cmd_mux);
  requested = s_ui_record_stop_requested;
  s_ui_record_stop_requested = false;
  portEXIT_CRITICAL(&s_ui_cmd_mux);

  return requested;
}

/**
 * Discard pending UI record commands in states where neither command is valid.
 *
 * Inputs: None.
 * Returns: None.
 */
static void ui_clear_record_requests_(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_record_start_requested = false;
  s_ui_record_stop_requested = false;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Apply pending UI configuration/control requests from the State task. */
static void ui_apply_control_requests_(void){
  ui_control_requests_t req = {};

  portENTER_CRITICAL(&s_ui_cmd_mux);
  req = s_ui_control_requests;
  s_ui_control_requests = {};
  portEXIT_CRITICAL(&s_ui_cmd_mux);

  if(req.language_toggle){
    settings_t settings = {};
    if(settings_get(&settings)){
      const language_t next = (settings.language == LANGUAGE_FRENCH)
                                ? LANGUAGE_ENGLISH : LANGUAGE_FRENCH;
      (void)settings_set_language(next);
    }
  }

  if(req.set_date){
    rtc_datetime_t dt = {};
    if(datetime_service_get(&dt)){
      dt.year = req.year;
      dt.month = req.month;
      dt.day = req.day;
      if(datetime_service_set(&dt)){
        (void)settings_set_date_set(true);
      }
    }
  }

  if(req.set_time){
    rtc_datetime_t dt = {};
    if(datetime_service_get(&dt)){
      dt.hour = req.hour;
      dt.min = req.minute;
      dt.sec = 0u;
      if(datetime_service_set(&dt)){
        (void)settings_set_time_set(true);
      }
    }
  }

  if(req.set_registration){
    (void)settings_set_registration(req.registration);
  }

  if(req.auto_recording_toggle){
#if !(AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY && AUTOMATION_DIAGNOSTIC_FORCE_ALL_ON)
    settings_t settings = {};
    if(settings_get(&settings)){
      (void)settings_set_auto_recording(!settings.auto_recording);
    }
#endif
  }

  if(req.auto_wifi_toggle){
#if !(AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY && AUTOMATION_DIAGNOSTIC_FORCE_ALL_ON)
    settings_t settings = {};
    if(settings_get(&settings)){
      const bool enable = !settings.auto_wifi;
      if(settings_set_auto_wifi(enable) && enable &&
         (s_st.state == ST_READY) && settings_is_complete(&settings)){
#if !AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY
        // Apply an AUTO WIFI enable immediately when selected in READY.
        // READY exit still disables WiFi before STARTING.
        web_task_set_enabled(true);
#endif
      }
    }
#endif
  }

  if(req.auto_delete_toggle){
#if !(AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY && AUTOMATION_DIAGNOSTIC_FORCE_ALL_ON)
    settings_t settings = {};
    if(settings_get(&settings)){
      (void)settings_set_auto_delete(!settings.auto_delete);
    }
#endif
  }

  if(req.wifi_toggle && (s_st.state == ST_READY)){
    if(web_task_is_enabled()){
      web_task_set_enabled(false);
    }else{
      settings_t settings = {};
      if(settings_get(&settings) && settings_is_complete(&settings)){
        web_task_set_enabled(true);
      }
    }
  }
}

// =============================================================================
// Time helpers
// =============================================================================

/**
 * Now ms performs the state task operation represented by this function and
 * keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
static uint32_t now_ms(void){
  // State machine elapsed time shall use FreeRTOS tick timebase.
  const TickType_t t = xTaskGetTickCount();
  return (uint32_t)(t * portTICK_PERIOD_MS);
}

// =============================================================================
// Status/message helpers
// =============================================================================

// Message ownership rule:
// - s_st.message_id stores the nominal state message.
// - state_task_get_status() overlays an active error-manager message when needed.
// - ui_task renders the effective message and shall not choose error messages.


// =============================================================================
// Message helpers
// =============================================================================

/**
 * Updates set msg state and applies the change to the owning module or
 * hardware interface.
 *
 * Inputs: `id`.
 * Returns: None.
 */
static void set_msg(msg_id_t id){
  // State task shall publish message by updating status snapshot.
  if(s_st.message_id != id){
    s_st.message_id = id;
  }
}


// =============================================================================
// State transition helper
// =============================================================================


/**
 * Default msg for state performs the state task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `st`.
 * Returns: Message identifier selected for display.
 */
static msg_id_t default_msg_for_state(recorder_state_t st){
  switch(st){
    case ST_BOOT:      return MSG_BOOT;
    case ST_READY:     return MSG_READY;
    case ST_STARTING:  return MSG_STARTING;
    case ST_RECORDING: return MSG_RECORDING;
    case ST_STOPPING:  return MSG_STOPPING;
    case ST_OFF:       return MSG_SHUTDOWN;
    case ST_ERROR:     return MSG_ERROR;
    default:           return MSG_NONE;
  }
}

/**
 * Updates state set state and applies the change to the owning module or
 * hardware interface.
 *
 * Inputs: `st`.
 * Returns: None.
 */
static void state_set(recorder_state_t st){
  // Only state_set() changes the state.
  // first_pass shall be set only on state change.
  if(s_st.state != st){
    s_st.state = st;
    s_first_pass = true;
    s_entry_ms = now_ms();
    s_state_tick = 0u;
    // Error audio is enabled only while the recorder is in ST_ERROR.
    if(st != ST_ERROR){
      audio_alert_service_set_error(false, 0u);
    }
    // On state change, publish the default state message.
    set_msg(default_msg_for_state(st));
  }
}


// =============================================================================
// Error-display helpers
// =============================================================================


/**
 * Updates whether the currently active error may be acknowledged or cleared by
 * the operator.
 *
 * Inputs: None.
 * Returns: None.
 */
static void update_error_clearable(void){
  const error_code_t active = error_manager_get_active();
  bool clearable = false;

  if(active != ERR_NONE){
    clearable = error_manager_is_sd_error(active) ? sd_error_show_ok_clear() : true;
  }

  error_manager_set_clearable(clearable);
}

/**
 * Handles the operator clear action for active errors, including the two-step
 * SD recovery acknowledge path.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool handle_error_clear_request(bool clear_requested){
  const error_code_t active = error_manager_get_active();

  if(error_manager_is_sd_error(active)){
    // SD errors use a two-step recovery:
    // 1) when sd_task says the condition is recoverable, the operator clear
    //    request acknowledges the SD task;
    // 2) state_task waits until sd_task has actually cleared its SD error.
    if(clear_requested && sd_error_show_ok_clear()){
      sd_request_ack_error();
      return true;
    }

    if(sd_error_get() == ERR_NONE){
      error_manager_clear_active();
      state_set(ST_READY);
      return true;
    }

    return false;
  }

  // Non-SD errors are cleared directly only when error_manager metadata says
  // the active error is recoverable and the operator requested clear.
  if((error_manager_can_clear() != true) || !clear_requested){
    return false;
  }

  error_manager_clear_active();
  state_set(ST_READY);
  return true;
}

// SD state is queried via narrow helpers in sd_task.h; raw SD internals stay private to sd_task.

// -----------------------------------------------------------------------------
// Periodic hardware snapshot helpers
// -----------------------------------------------------------------------------

static bool s_battery_low_cached = false;
static bool s_low_battery_shutdown_requested = false;
static bool s_low_battery_notice_active = false;

/**
 * Updates the published USB status snapshot used by UI display and power
 * management. USB removal shutdown is detected locally in ST_READY.
 *
 * Inputs: None.
 * Returns: None.
 */
static void update_usb_status_snapshot(void){
  if(!pmu_ok){
    s_st.usb_present = false;
    s_st.usb_present_valid = false;
    return;
  }

  bool usb_now = false;
  const bool ok = usb_present(&usb_now);
  s_st.usb_present_valid = ok;
  if(ok){
    s_st.usb_present = usb_now;
  }
}

/**
 * Updates the published battery percentage and cached low-battery status used
 * by UI display and battery-protection logic.
 *
 * Inputs: None.
 * Returns: None.
 */
static void update_battery_snapshot(void){
  if(!pmu_ok){
    s_st.battery_percent_valid = false;
    s_battery_low_cached = false;
    return;
  }

  uint8_t pct = 0u;
  const bool ok = battery_percent(&pct);
  s_st.battery_percent_valid = ok;
  if(ok){
    s_st.battery_percent = pct;
    s_battery_low_cached = ((uint16_t)pct <= (uint16_t)PMU_BATT_LOW_THRESHOLD_PCT);
  } else {
    s_battery_low_cached = false;
    error_manager_raise(ERR_PMU_FAULT);
  }
}



// =============================================================================
// Continuous acceleration acquisition
// =============================================================================

/**
 * Acquire one normal 20 Hz acceleration sample from the State-task context.
 *
 * Signal processing runs continuously through READY/STARTING/RECORDING/STOPPING.
 * Recording block formation and ring-buffer updates remain strictly conditional
 * on ST_RECORDING, preserving the existing recorder data path. Calibration keeps
 * its existing direct accelerometer ownership and temporarily suspends this path.
 */
static void acceleration_service_(bool recording_enabled, bool suspended){
  static bool was_suspended = true;

  if(suspended){
    if(!was_suspended){
      automation_service_reset_signal_history();
    }
    was_suspended = true;
    return;
  }

  if(was_suspended){
    // Samples separated by BOOT/ERROR/calibration are not contiguous. Start a
    // fresh causal history when normal 20 Hz acquisition resumes.
    automation_service_reset_signal_history();
    was_suspended = false;
  }

  if(!accel_ok){
    return;
  }

  accel_sample_t sample = {};
  int32_t ts_ms = 0;
  if(!accel_read_xyz_bounded(&sample, &ts_ms)){
    // READY automation can tolerate an occasional failed sample. During an
    // actual recording, preserve the existing fatal acquisition semantics.
    if(recording_enabled){
      error_manager_raise(ERR_ACCEL_NO_RESPONSE);
    }
    return;
  }

  bool automation_logic_recording = recording_enabled;
#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
  if(recording_enabled && s_diag_observe_only_recording){
    // In the all-day observe-only file, per-record flight logic is active only
    // inside a virtual AUTO RECORD session. The physical manual recording
    // remains open and is never controlled by automation.
    automation_logic_recording =
        s_diag_virtual_auto_mode && s_diag_virtual_session_active;
  }
#endif
  automation_service_update(&sample, automation_logic_recording);

  if(!recording_enabled){
    return;
  }

#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
  // Recorder-side consumers are finished with this sample. Only three
  // precomputed integer offsets are applied before the ring push; all event
  // detection/queuing is deferred until after the push.
  diagnostic_apply_prepared_overlay_(&sample);
#endif

  record_block_t block = {};
  record_format_build_block(&block, ts_ms, &sample);
  if(!ring_buffer_push(&block)){
    error_manager_raise(ERR_RINGBUFFER_OVERFLOW);
    return;
  }

#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
  // The pending code has now been committed to the recording stream. The next
  // code will be selected later in diagnostic_update_after_cycle_().
  s_diag_next_wire_code = DIAG_NONE;
  s_diag_next_axis_offset_mg[0] = 0;
  s_diag_next_axis_offset_mg[1] = 0;
  s_diag_next_axis_offset_mg[2] = 0;
#endif

  watchdog_kick(WD_RECORD);
}

// =============================================================================
// Main task loop
// =============================================================================


/**
 * READY exit cleanup consumes READY-only latches and disables support
 * functions that shall not remain active outside READY.
 *
 * Inputs: None.
 * Returns: None.
 */
static inline void ready_exit_cleanup(void){
  // USB-loss warning is a READY-only automation alert.
  audio_alert_service_set_usb_power_loss(false);
  // On exit from READY, WiFi/Web shall be OFF.
  web_task_set_enabled(false);
  // SD file-management shall be disabled outside READY.
  // Disable touch when leaving READY. RECORDING re-enables touch on entry so
  // display standby can wake from touch while recording.
  touch_enable(false);
}

/**
 * Reports whether an SD error is a maintenance condition that can be resolved
 * from READY using MENU/START WIFI/Web file archive.
 *
 * Inputs: `err`.
 * Returns: `true` for SD max-file-count maintenance; otherwise `false`.
 */
static bool sd_maintenance_error_(error_code_t err){
  return (err == ERR_SD_FILES_FULL);
}

/**
 * Converts an SD maintenance condition to the corresponding user-visible
 * message.
 *
 * Inputs: `err`.
 * Returns: Maintenance message identifier, or `MSG_NONE` if not maintenance.
 */
static msg_id_t sd_maintenance_msg_(error_code_t err){
  if(err == ERR_SD_FILES_FULL){
    return MSG_SD_FULL_FILES;
  }
  return MSG_NONE;
}

/**
 * Reports whether the recorder shall shut down for low battery.
 *
 * The configured 5% threshold is absolute. When reached, the recorder closes
 * any active recording and shuts down regardless of USB, WiFi, or automation.
 *
 * Inputs: None.
 * Returns: `true` when the low-battery shutdown path shall be entered.
 */
static bool automation_diag_force_all_on_(void){
#if AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY && AUTOMATION_DIAGNOSTIC_FORCE_ALL_ON
  return true;
#else
  return false;
#endif
}

static bool effective_auto_recording_(bool settings_loaded, const settings_t &settings){
  return automation_diag_force_all_on_() || (settings_loaded && settings.auto_recording);
}

static bool effective_auto_wifi_(bool settings_loaded, const settings_t &settings){
  return automation_diag_force_all_on_() || (settings_loaded && settings.auto_wifi);
}

static bool effective_auto_delete_(bool settings_loaded, const settings_t &settings){
  return automation_diag_force_all_on_() || (settings_loaded && settings.auto_delete);
}

static bool low_battery_shutdown_required_(void){
  // The 5% threshold is an absolute battery-protection limit. It is not
  // cancelled by USB presence, WiFi state, automation, or active recording.
  return s_battery_low_cached;
}

/**
 * Return whether the current recording shall be discarded after a clean close.
 * Only automatically started recordings are eligible; manual recordings are
 * always retained. The live detector has already accumulated the evidence, so
 * no post-close analysis state is required.
 */
static bool auto_delete_current_recording_(void){
#if AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY
  // Field diagnostic build is strictly observe-only: AUTO DELETE decisions
  // are logged by the virtual policy trace but never remove a physical file.
  return false;
#endif
  if(!s_recording_automatic){
    return false;
  }

  settings_t settings = {};
  const bool settings_loaded = settings_get(&settings);
  if(!effective_auto_delete_(settings_loaded, settings)){
    return false;
  }

  return !automation_service_status().flight_seen;
}

/**
 * Requests the user-visible low-battery notice before PMU shutdown.
 *
 * Inputs: None.
 * Returns: None.
 */
static void request_low_battery_shutdown_(void){
  s_low_battery_shutdown_requested = true;
  s_low_battery_notice_active = true;
  web_task_set_enabled(false);
  touch_enable(false);
  state_set(ST_OFF);
  set_msg(MSG_LOW_BATT);
}

/**
 * Low-power shutdown service enforces the absolute battery-protection threshold
 * from every recorder state.
 *
 * Inputs: None.
 * Returns: None.
 */
static void low_power_shutdown_service_(void){
  if(!low_battery_shutdown_required_()){
    return;
  }

  switch(s_st.state){
    case ST_OFF:
      if(s_low_battery_notice_active){
        set_msg(MSG_LOW_BATT);
      }
      return;

    case ST_RECORDING:
    case ST_STARTING:
      s_shutdown_after_stop_requested = true;
      s_low_battery_shutdown_requested = true;
      sd_request_close(auto_delete_current_recording_());
      state_set(ST_STOPPING);
      return;

    case ST_STOPPING:
      s_shutdown_after_stop_requested = true;
      s_low_battery_shutdown_requested = true;
      return;

    case ST_READY:
      ready_exit_cleanup();
      request_low_battery_shutdown_();
      return;

    case ST_BOOT:
    case ST_ERROR:
    default:
      request_low_battery_shutdown_();
      return;
  }
}

/**
 * State task main loop owns the recorder high-level state machine, coordinates
 * setup locks, start/stop/shutdown behavior, and schedules periodic
 * housekeeping.
 *
 * Inputs: `arg`.
 * Returns: None.
 */
static void state_task_main(void *arg){
  (void)arg;

  TickType_t last_wake = xTaskGetTickCount();

  // READY-local USB transition detector. Current USB presence is still
  // published in all states by low-rate housekeeping.
  static bool s_ready_usb_prev_present = false;
  static bool s_ready_usb_prev_valid = false;

  // Battery-powered WiFi is permitted only to support recorder calibration.
  // The timer runs while WiFi is enabled, USB is known absent, and no recorder
  // calibration session is active. An active calibration clears the timer so
  // a fresh full grace period starts when calibration ends.
  static bool s_wifi_battery_idle_timer_active = false;
  static uint32_t s_wifi_battery_idle_since_ms = 0u;

  // One-time state-task runtime initialization.  This establishes the
  // state-task-owned status snapshot and resets services coordinated here
  // before the periodic state machine starts.
  state_set(ST_BOOT);
  error_manager_clear_active();
  error_manager_set_clearable(false);

  // Recorder-core service reset/initialization.  The SD task owns SD storage;
  // state_task owns high-level state, button semantics, and timebase start.
  ring_buffer_init();
  button_init();
  timebase_init();
  datetime_service_init();
  (void)calibration_service_init();
  automation_service_init();

  s_watchdog_ack_pending = watchdog_persistent_fault_present();
  if(s_watchdog_ack_pending){
    init_power_button();
    set_msg(MSG_FATAL_WDG_CLR);
  }

  // Select the initial operator message.  Incomplete settings do not block
  // BOOT forever and are not treated as a fatal hardware error.
  if(!s_watchdog_ack_pending){
    if(!settings_storage_ok){
      set_msg(MSG_SETTINGS_LOCKED);
    } else {
      settings_t settings = {};
      if(!settings_get(&settings) || !settings_is_complete(&settings)){
        set_msg(MSG_SETTINGS_LOCKED);
      }
    }
  }

  s_first_pass = false;
  publish_status_snapshot_();

  for(;;){
    // Task runs periodically.  State-specific work is performed first; lower-rate
    // housekeeping is intentionally deferred to the end of the loop.
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CFG_STATE_TASK_PERIOD_MS));

    const uint32_t now = now_ms();

    watchdog_kick(WD_STATE);
    watchdog_set_required(WD_RECORD, s_st.state == ST_RECORDING);

    if(s_watchdog_ack_pending){
      set_msg(MSG_FATAL_WDG_CLR);

      if(test_power_button(POWER_CLEAR_HOLD_MS) == true){
        watchdog_persistent_fault_clear();
        s_watchdog_ack_pending = false;
        state_set(ST_BOOT);
      }

      publish_status_snapshot_();
      continue;
    }

    calibration_session_service(now);

    // Calibration keeps its established direct accelerometer ownership. In all
    // normal operational states the State task performs one 20 Hz acquisition;
    // only ST_RECORDING formats/pushes the resulting recording block.
    const bool calibration_acquisition_active =
        calibration_session_active() || calibration_installation_session_active();
    const bool normal_acquisition_state =
        (s_st.state == ST_READY) || (s_st.state == ST_STARTING) ||
        (s_st.state == ST_RECORDING) || (s_st.state == ST_STOPPING);
    acceleration_service_(s_st.state == ST_RECORDING,
                          calibration_acquisition_active || !normal_acquisition_state);

    // Tick counter since last transition; used for simple periodic actions.
    s_state_tick++;

    // UI only posts requests. Runtime settings persistence and WiFi enable
    // decisions are applied here so State remains the single authority.
    ui_apply_control_requests_();

    switch(s_st.state){

      case ST_BOOT: {

        // Record start/stop UI commands are not applicable in this state.
        ui_clear_record_requests_();

        // Purpose: Initialize services/hardware after power-up.

        // Recurring actions
        // Attempt at most one hardware initialization per tick, in a fixed order.
        // This avoids repeatedly accessing several I2C devices in the same cycle
        // and keeps BOOT timing easier to reason about.
        if(!pmu_ok){
          pmu_init();
        } else if(!rtc_ok){
          rtc_init();
        } else if(!touch_ok){
          touch_init();
        } else if(!accel_ok){
          accel_init();
        }

        // Evaluate settings readiness once for this BOOT cycle. Incomplete
        // settings are allowed to enter READY, but recording remains locked.
        settings_t settings = {};
        const bool settings_loaded = settings_storage_ok && settings_get(&settings);
        const bool settings_ready = settings_loaded && settings_is_complete(&settings);

        // Hardware readiness is separate from settings readiness. Hardware
        // failure can become a BOOT error; incomplete settings cannot.
        const bool hw_ready = pmu_ok && rtc_ok && touch_ok && accel_ok;

        // State change actions
        // SD task owns SD boot/recovery. Checking here is necessary because
        // BOOT can be entered again after SD error recovery. State task only
        // consumes the reported SD error and converts it into a user-visible error.
        const error_code_t sd_err = sd_error_get();
        if(sd_err != ERR_NONE){
          if(sd_maintenance_error_(sd_err)){
            state_set(ST_READY);
            set_msg(sd_maintenance_msg_(sd_err));
          } else {
            error_manager_raise(sd_err);
            state_set(ST_ERROR);
          }
          break;
        }

        // Once required hardware is ready and the SD task is idle/closed, the
        // high-level recorder can leave BOOT. Missing settings only select the
        // locked READY message; they do not keep the unit in BOOT.
        if(hw_ready && sd_is_closed()){
          state_set(ST_READY);
          if(!settings_ready){
            set_msg(MSG_SETTINGS_LOCKED);
          }
          break;
        }

        // While BOOT is still waiting for hardware or SD readiness, keep the
        // settings-lock message visible if setup is incomplete.
        if(!settings_ready){
          set_msg(MSG_SETTINGS_LOCKED);
        }

        // If BOOT exceeds its timeout while hardware is still unavailable,
        // transition to ST_ERROR with the most relevant error. Incomplete
        // settings are not treated as a fatal boot error.
        if((now - s_entry_ms) > CFG_BOOT_TIMEOUT_MS){
          if(!hw_ready){
            error_code_t err = ERR_FATAL_GENERIC;
            if(!pmu_ok){
              err = ERR_PMU_FAULT;
            } else if(!rtc_ok){
              err = ERR_RTC_INVALID;
            } else if(!touch_ok){
              err = ERR_TOUCH_FAULT;
            } else if(!accel_ok){
              err = ERR_ACCEL_NO_RESPONSE;
            }
            error_manager_raise(err);
            state_set(ST_ERROR);
            break;
          }
        }
        break;

      }

      case ST_READY: {

        // Purpose: Idle operational state with user interaction enabled.

        // Entry actions
        if(s_first_pass){
          // On entry to READY, UI shall be on the main page.
          // NOTE: UI-page forcing is a UI responsibility in this baseline.
          // If needed later, add an explicit ui_request_main_page() hook here.

          // Touch shall be enabled in READY.
          touch_enable(true);

          // AUTO WIFI is a READY-entry action only. It uses the same existing
          // Web/WiFi enable path as manual control; leaving READY turns WiFi OFF
          // through ready_exit_cleanup(), including READY -> STARTING.
          settings_t ready_settings = {};
          const bool ready_settings_loaded = settings_storage_ok && settings_get(&ready_settings);
          const bool auto_wifi_on = ready_settings_loaded &&
                                    settings_is_complete(&ready_settings) &&
                                    effective_auto_wifi_(ready_settings_loaded, ready_settings);
#if AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY
          // Field diagnostic build: AUTO WIFI is forced logically ON for the
          // virtual policy trace, but it cannot actuate the real Web/AP.
          (void)auto_wifi_on;
#else
          web_task_set_enabled(auto_wifi_on);
#endif

          // READY entry resets state-task-owned stop/error latches only. It
          // shall not directly clear SD-task internal state.
          s_shutdown_after_stop_requested = false;
          s_low_battery_shutdown_requested = false;
          s_low_battery_notice_active = false;
          s_recording_automatic = false;
          automation_service_reset_start_confirmation();
          error_manager_clear_active();

          // READY is the normal state where the UI can edit date/time settings.
          // Refresh the application date/time cache from RTC on entry unless a
          // local settings edit is waiting to be written back to RTC.
          (void)datetime_service_sync_rtc();

          // Initialize READY-local USB transition detection.  USB already
          // absent when entering/re-entering READY is only the reference
          // state; it is not a USB-removal event.
          update_usb_status_snapshot();
          s_ready_usb_prev_present = s_st.usb_present;
          s_ready_usb_prev_valid = s_st.usb_present_valid;
          s_wifi_battery_idle_timer_active = false;
          s_wifi_battery_idle_since_ms = 0u;

          // Initialize hold-based button detectors for READY semantics.
          // READY uses record-start hold, clear-settings gesture detection,
          // and power-long hold for shutdown.
          init_record_button();
          init_power_button();
          s_first_pass = false;
        }

        // Recurring actions

        // Touch: refresh input snapshot each tick for UI responsiveness.
        if(touch_is_enabled()){
          touch_service_update_from_hw();
        }

        // Settings are changed only by this State task through latched UI requests.
        // Incomplete settings keep READY active but prevent recording start.
        settings_t settings = {};
        const bool settings_loaded = settings_storage_ok && settings_get(&settings);
        const bool settings_ready = settings_loaded && settings_is_complete(&settings);
        calibration_service_refresh_status();
        calibration_service_publish_driver_state();
        const calibration_status_t cal_status = calibration_service_status();
        const bool installation_ready = calibration_service_installation_valid();
        const bool calibration_ready = calibration_service_is_recording_allowed();

        // SD max-file-count maintenance blocks recording but keeps
        // READY/menu/WiFi available so the operator can archive files.
        const error_code_t sd_err = sd_error_get();
        const bool sd_maintenance_needed = sd_maintenance_error_(sd_err);

        // Record-button actions are time-triggered while the button is still held.
        // A qualified hardware record hold starts recording normally, unless
        // the power button is pressed at the same time, in which case the
        // combined hardware gesture clears settings and shuts the unit down.
        // A UI START RECORD request enters the same normal start path, but does
        // not participate in the clear-settings gesture.
        const bool wifi_active = web_task_is_enabled();
        const bool power_pressed = power_button_pressed();
        const bool record_requested = test_record_button(RECORD_START_HOLD_MS);
        const bool ui_start_requested = ui_take_record_start_request_();
        (void)ui_take_record_stop_request_(); // STOP is meaningful only in RECORDING.
        const bool clear_settings_requested = record_requested && power_pressed;

        // When WiFi/Web access is active, the touch START RECORD button is
        // disabled by the UI.  The physical RECORD button intentionally keeps
        // authority so recording can always be started independently of the
        // UI/Web layer.  Leaving READY for STARTING forces WiFi/Web OFF.
        const bool manual_start_requested =
            (record_requested && (!power_pressed)) ||
            ((!wifi_active) && ui_start_requested);
        const bool automation_selected = effective_auto_recording_(settings_loaded, settings);
        const bool automation_policy_enabled =
            automation_selected && (AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY == 0);
        const bool automation_armed =
            settings_ready && calibration_ready && (!sd_maintenance_needed) &&
            (!calibration_acquisition_active) && automation_policy_enabled;
        const automation_status_t automation_status = automation_service_status();
        const bool automatic_start_requested =
            automation_armed &&
            (automation_status.motion_start_confirmed ||
             automation_status.attitude_start_confirmed);

        // Keep setup-lock messages visible until all required setup is complete.
        // Settings are checked first, then calibration status.
        if(!settings_ready){
          set_msg(MSG_SETTINGS_LOCKED);
        } else if(cal_status == CAL_STATUS_FAULT){
          set_msg(MSG_CALIBRATION_FAULT);
        } else if(cal_status != CAL_STATUS_VALID){
          set_msg(MSG_ACCEL_CALIBRATION_REQUIRED);
        } else if(!installation_ready){
          set_msg(MSG_INSTALLATION_CALIBRATION_REQUIRED);
        } else if(sd_maintenance_needed){
          set_msg(sd_maintenance_msg_(sd_err));
        } else if((s_st.message_id == MSG_SETTINGS_LOCKED) ||
                  (s_st.message_id == MSG_ACCEL_CALIBRATION_REQUIRED) ||
                  (s_st.message_id == MSG_INSTALLATION_CALIBRATION_REQUIRED) ||
                  (s_st.message_id == MSG_CALIBRATION_FAULT) ||
                  (s_st.message_id == MSG_SD_LOW_SPACE) ||
                  (s_st.message_id == MSG_SD_FULL_FILES)){
          set_msg(MSG_READY);
        }

        // State change actions
        // READY still monitors SD errors because SD task may detect an SD fault
        // while idle, before recording is requested. Max root-file-count is a
        // READY/Web maintenance condition. Low free space is not, because
        // archiving root files to /processed does not free SD memory.
        if((sd_err != ERR_NONE) && !sd_maintenance_needed){
          error_manager_raise(sd_err);
          ready_exit_cleanup();
          state_set(ST_ERROR);
          break;
        }
        // In READY, only a fresh USB-present to USB-absent transition requests
        // shutdown. USB removal that happened before READY entry is ignored.
        const bool trig_usb =
            s_ready_usb_prev_valid &&
            s_st.usb_present_valid &&
            s_ready_usb_prev_present &&
            (!s_st.usb_present);
        s_ready_usb_prev_present = s_st.usb_present;
        s_ready_usb_prev_valid = s_st.usb_present_valid;

        const bool trig_pwr  = (test_power_button(POWER_SHUTDOWN_HOLD_MS) == true);

        // WiFi on battery is a special recorder-calibration mode. All other
        // Web maintenance functions require USB power at their HTTP/API gate.
        // If WiFi is left on without an active recorder calibration, shut down
        // after the configured grace period. Installation calibration does not
        // inhibit this timer because it is a USB-powered maintenance function.
        const bool wifi_on_battery =
            wifi_active &&
            s_st.usb_present_valid &&
            (!s_st.usb_present);
        const bool recorder_calibration_active = calibration_session_active();

        if((!automation_policy_enabled) && wifi_on_battery && (!recorder_calibration_active)){
          if(!s_wifi_battery_idle_timer_active){
            s_wifi_battery_idle_timer_active = true;
            s_wifi_battery_idle_since_ms = now;
          } else if(((uint32_t)(now - s_wifi_battery_idle_since_ms)) >=
                    (uint32_t)WIFI_BATTERY_IDLE_SHUTDOWN_MS){
            ready_exit_cleanup();
            state_set(ST_OFF);
            break;
          }
        } else {
          // USB power, WiFi OFF, or active recorder calibration all cancel the
          // battery-WiFi idle timer. When calibration later ends, a new full
          // grace period starts.
          s_wifi_battery_idle_timer_active = false;
          s_wifi_battery_idle_since_ms = 0u;
        }

        // AUTO RECORDING deliberately allows READY to remain powered from the
        // internal battery. While USB is absent in READY, give a persistent
        // double-beep warning so the ground operator is prompted to restore
        // external power. Leaving READY stops the warning; therefore recording
        // itself is never accompanied by this alert. The independent <=5%
        // battery shutdown remains authoritative.
        const bool usb_power_loss_alert =
            automation_policy_enabled &&
            s_st.usb_present_valid &&
            (!s_st.usb_present);
        audio_alert_service_set_usb_power_loss(usb_power_loss_alert);

        const bool trig_usb_shutdown = trig_usb && (!wifi_active) && (!automation_policy_enabled);

        if(trig_usb_shutdown || trig_pwr){
          ready_exit_cleanup();
          state_set(ST_OFF);
          break;
        }

        // Combined record-hold plus power-pressed gesture clears user settings
        // and cancels calibration sessions, then shuts down. Recorder and
        // installation calibration histories are not erased by this gesture.
        if(clear_settings_requested){
          const bool settings_cleared = settings_clear();
          const bool calibration_sessions_cancelled = calibration_service_clear();

          if(settings_cleared && calibration_sessions_cancelled){
            ready_exit_cleanup();
            state_set(ST_OFF);
          } else {
            error_manager_raise(ERR_FATAL_GENERIC);
            ready_exit_cleanup();
            state_set(ST_ERROR);
          }
          break;
        }

        if(settings_ready && calibration_ready && (!sd_maintenance_needed) &&
           (manual_start_requested || automatic_start_requested)){
          // Manual start wins if both requests qualify on the same tick. Reset
          // only the per-record flight evidence; causal filter/RMS history stays
          // continuous across the recording boundary.
          s_recording_automatic = (!manual_start_requested) && automatic_start_requested;
#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
          s_diag_observe_only_recording =
              manual_start_requested && (AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY != 0);
          s_diag_virtual_auto_mode =
              s_diag_observe_only_recording && automation_selected;
          s_diag_virtual_session_active = false;
          if(manual_start_requested){
            diagnostic_enqueue_extended_(DIAG_EXT_MANUAL_RECORD_START);
          }
          s_diag_start_motion_policy =
              s_recording_automatic && automation_status.motion_start_confirmed;
          s_diag_start_attitude_policy =
              s_recording_automatic && automation_status.attitude_start_confirmed;
#endif
          automation_service_begin_recording();
          automation_service_reset_start_confirmation();
          ready_exit_cleanup();
          state_set(ST_STARTING);
          break;
        }
        break;
      }


      case ST_STARTING: {

        // Record start/stop UI commands are not applicable in this state.
        ui_clear_record_requests_();

        // Purpose: Transient state requesting SD to open the recording file.

        // Entry actions
        if(s_first_pass){
          // Capture recording timebase token for filename/metadata from the
          // application date/time cache. The cache is synchronized with RTC by
          // state-task housekeeping while READY/idle.
          rtc_datetime_t dt_now = {};
          if(datetime_service_get(&dt_now)){
            (void)timebase_mark_record_start(&dt_now);
          } else {
            error_manager_raise(ERR_RTC_INVALID);
            state_set(ST_ERROR);
            break;
          }

          // Reset the recorder buffer immediately before requesting file open
          // so stale samples from a previous session cannot enter the new file.
          ring_buffer_reset();

          sd_request_open();
          s_first_pass = false;
        }

        // State change actions
        // SD_OPEN is owned by sd_task. When sd_task reports the file open,
        // recording can begin unless an SD open error is reported below.
        if(sd_is_open()){
#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
          diagnostic_begin_recording_();
#endif
          state_set(ST_RECORDING);
          break;
        }

        const error_code_t sd_err = sd_error_get();
        if(sd_err != ERR_NONE){
          if(sd_maintenance_error_(sd_err)){
            set_msg(sd_maintenance_msg_(sd_err));
            state_set(ST_READY);
          } else {
            error_manager_raise(sd_err);
            state_set(ST_ERROR);
          }
          break;
        }
        break;
      }


      case ST_RECORDING: {

        // Purpose: Acquire samples and record to SD via ring buffer + SD task.

        // Entry actions
        if(s_first_pass){
          // Initialize hold-based button detectors for RECORDING semantics.
          // RECORDING uses record-stop hold and power-long hold.
          init_record_button();
          init_power_button();

          // Touch is enabled during RECORDING so display standby can wake from
          // touch without affecting acquisition or SD writing.
          touch_enable(true);
          s_first_pass = false;
        }

        // Recurring actions
        // Refresh touch snapshot for display-standby wake while recording.
        if(touch_is_enabled()){
          touch_service_update_from_hw();
        }

        // The common 20 Hz acceleration service already acquired this tick's
        // sample before the state switch and, because state is RECORDING, built
        // the 0x70 block and pushed it to the existing ring buffer.

        // State change actions
        // Faults are evaluated before operator stop requests so the recorded
        // error cause is not hidden by a simultaneous button action.
        // 1) Non-SD recording faults raised by the acceleration service stop
        // recording immediately. SD errors are handled below through sd_task.
        const error_code_t active_err = error_manager_get_active();
        if((active_err != ERR_NONE) && !error_manager_is_sd_error(active_err)){
          state_set(ST_ERROR);
          break;
        }

        // 2) SD task owns SD error classification.
        const error_code_t sd_err = sd_error_get();
        if(sd_err != ERR_NONE){
          if(sd_maintenance_error_(sd_err)){
            state_set(ST_READY);
            set_msg(sd_maintenance_msg_(sd_err));
          } else {
            error_manager_raise(sd_err);
            state_set(ST_ERROR);
          }
          break;
        }

        // 3) Stop recording conditions -> STOPPING

        // User stop: hardware record-stop hold or UI STOP RECORD request
        // closes the file and returns to READY through the normal STOPPING
        // path.
        (void)ui_take_record_start_request_(); // START is meaningful only in READY.
        const bool ui_stop_requested = ui_take_record_stop_request_();
        if((test_record_button(RECORD_STOP_HOLD_MS) == true) || ui_stop_requested){
          state_set(ST_STOPPING);
          break;
        }

        // Power-long during recording first closes the SD file, then
        // continues to shutdown from ST_STOPPING.
        if(test_power_button(POWER_SHUTDOWN_HOLD_MS) == true){
          s_shutdown_after_stop_requested = true;
          state_set(ST_STOPPING);
          break;
        }

        // Low battery uses the same close-then-shutdown path as power-long:
        // close the file through ST_STOPPING before entering ST_OFF.
        if(low_battery_shutdown_required_() == true){
          s_shutdown_after_stop_requested = true;
          s_low_battery_shutdown_requested = true;
          state_set(ST_STOPPING);
          break;
        }

        // Automatic stop applies only to an automatically started session. In the
        // v1.54 diagnostic build this physical policy is suppressed. Before flight_seen, the
        // existing 300 s quiet timeout closes nuisance/pre-flight recordings.
        // Once flight_seen latches, motion quiet can no longer stop the file;
        // only the dedicated ordered flight-end detector may do so.
        settings_t recording_settings = {};
        const bool recording_settings_loaded = settings_get(&recording_settings);
        const bool auto_recording_enabled =
            effective_auto_recording_(recording_settings_loaded, recording_settings);
        if(s_recording_automatic && auto_recording_enabled &&
           (AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY == 0)){
          const automation_status_t automation_status = automation_service_status();
          const bool automatic_stop_requested =
              automation_status.flight_seen
                  ? automation_status.flight_end_confirmed
                  : automation_status.motion_stop_confirmed;
          if(automatic_stop_requested){
            state_set(ST_STOPPING);
            break;
          }
        }
        break;
      }


      case ST_STOPPING: {

#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
        if(s_first_pass){
          // No further 0x70 samples will be written in STOPPING. Drop any
          // un-emitted events from the completed recording so they cannot leak
          // into the next file; READY events accumulated afterward remain fresh.
          diagnostic_queue_reset_();
          s_diag_virtual_auto_mode = false;
          s_diag_virtual_session_active = false;
          s_diag_observe_only_recording = false;
          s_diag_virtual_wifi_on = false;
          s_diag_virtual_wifi_wait_quiet = false;
          s_diag_virtual_wifi_quiet_count = 0u;
        }
#endif

        // Record start/stop UI commands are not applicable in this state.
        ui_clear_record_requests_();

        // Purpose: Transient state requesting SD to close the recording file.
        // Exit is selected after the SD task reports closed:
        // - normal stop returns to READY;
        // - power/low-battery stop continues to OFF;
        // - SD close error transitions to ERROR.

        // Entry actions
        if(s_first_pass){
          touch_enable(false);
          // The live detector already knows whether an automatically started
          // session contained flight evidence. If AUTO DELETE is selected, the
          // SD task closes the file normally then removes both .bin and .sha.
          sd_request_close(auto_delete_current_recording_());
          s_first_pass = false;
        }

        // State change actions
        if(sd_is_closed()){
          if(s_shutdown_after_stop_requested){
            s_shutdown_after_stop_requested = false;
            if(s_low_battery_shutdown_requested){
              request_low_battery_shutdown_();
            } else {
              state_set(ST_OFF);
            }
          } else {
            state_set(ST_READY);
          }
          break;
        }
        const error_code_t sd_err = sd_error_get();
        if(sd_err != ERR_NONE){
          if(sd_maintenance_error_(sd_err)){
            set_msg(sd_maintenance_msg_(sd_err));
            state_set(ST_READY);
          } else {
            error_manager_raise(sd_err);
            state_set(ST_ERROR);
          }
          break;
        }
        // SD open/close timeout ownership lives in sd_task; this state only consumes classifier output.
        break;
      }


      case ST_ERROR: {

        // Record start/stop UI commands are not applicable in this state.
        ui_clear_record_requests_();

        // Purpose: Display error condition and wait for operator CLEAR.
        // SD errors are clearable only after sd_task re-probes SD status and
        // reports that the condition can be acknowledged.

        // Entry actions
        if(s_first_pass){
          // Initialize hold-based button detector for ERROR semantics.
          // ERROR uses power-clear hold for recoverable error acknowledgement
          // and power-long hold for shutdown.
          init_power_button();

          // Touch remains enabled in ERROR so the page-independent display
          // standby screen can always be woken by touching the display.
          touch_enable(true);
          audio_alert_service_set_error(true, (uint32_t)error_manager_get_active());
          s_first_pass = false;
        }

        // Refresh touch snapshot for display-standby wake while an error is
        // displayed.  Without this, a standby screen entered during an error
        // can no longer see the touch wake request.
        if(touch_is_enabled()){
          touch_service_update_from_hw();
        }

        // State change actions
        if(test_power_button(POWER_SHUTDOWN_HOLD_MS) == true){
          state_set(ST_OFF);
          break;
        }
        if(low_battery_shutdown_required_() == true){
          // Low-battery shutdown from ERROR still shows the recharge notice.
          request_low_battery_shutdown_();
          break;
        }

        // While displaying an SD error, keep the user-visible error aligned
        // with the latest SD-task classification. SD media state can change
        // while the system waits in ERROR.
        const error_code_t active_err = error_manager_get_active();
        audio_alert_service_set_error(true, (uint32_t)active_err);
        if(error_manager_is_sd_error(active_err)){
          const error_code_t current_sd_err = sd_error_get();

          // A recoverable SD error can transition into the file-count
          // maintenance condition after the operator acknowledges SD OK/CLR.
          // That condition must run in READY so MENU/START WIFI can be used to
          // archive root files to /processed.
          if(sd_maintenance_error_(current_sd_err)){
            error_manager_clear_active();
            state_set(ST_READY);
            set_msg(sd_maintenance_msg_(current_sd_err));
            break;
          }

          if((current_sd_err != ERR_NONE) && (current_sd_err != active_err)){
            error_manager_raise(current_sd_err);
            break;
          }
        }

        // Refresh whether the current error can be cleared, then consume an
        // operator clear request if the active error and recovery state allow it.
        update_error_clearable();

        const bool clear_requested = (test_power_button(POWER_CLEAR_HOLD_MS) == true);
        if(clear_requested){
          // PWR/CLR acknowledges the audible alert even when the underlying
          // error condition is still present and cannot yet be cleared.
          audio_alert_service_acknowledge();
        }

        if(handle_error_clear_request(clear_requested)){
          break;
        }
        break;
      }

      case ST_OFF: {

        // Record start/stop UI commands are not applicable in this state.
        ui_clear_record_requests_();

        // Purpose: Display the selected shutdown notice briefly, then request
        // PMU power down. Low-battery shutdown uses a longer recharge notice.
        // ST_OFF is terminal from the state-machine perspective. If
        // shutdown_device() returns, the state remains ST_OFF and retries.

        // Entry actions: none

        // Recurring actions: none

        // State change actions
        if(s_low_battery_notice_active){
          set_msg(MSG_LOW_BATT);
        }
        const uint32_t powerdown_delay_ms = s_low_battery_notice_active ?
            CFG_LOW_BATTERY_NOTICE_MS : CFG_POWERDOWN_DELAY_MS;
        if((now - s_entry_ms) > powerdown_delay_ms){
          shutdown_device();
        }
        break;
      }

      default:
        // Unexpected state shall transition to ERROR.
        error_manager_raise(ERR_FATAL_GENERIC);
        state_set(ST_ERROR);
        break;
    }

    // Perform low-rate housekeeping at the end of the loop so state-specific work,
    // including acceleration sampling in RECORDING, remains first.
    if((s_state_tick % CFG_STATE_HOUSEKEEPING_PERIOD_TICKS) == 0u){
      // Keep the shared date/time cache updated for the active UI, including
      // during RECORDING. Recording sample timestamps use the captured start
      // time plus the monotonic ESP timer, so recording correctness does not
      // depend on periodic RTC reads.
      (void)datetime_service_sync_rtc();

      update_usb_status_snapshot();
      update_battery_snapshot();
      low_power_shutdown_service_();
    }

#if AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED
    diagnostic_update_after_cycle_();
#endif

    publish_status_snapshot_();
  }
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Initializes state task init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void state_task_init(void){
  // Create the FreeRTOS state task once.
  if(s_task){ return; }

  const BaseType_t ok = xTaskCreatePinnedToCore(
      state_task_main,
      "state_task",
      CFG_STATE_TASK_STACK_WORDS,
      nullptr,
      CFG_STATE_TASK_PRIO,
      &s_task,
      CFG_STATE_TASK_CORE);

  if(ok != pdPASS){
    s_task = nullptr;
    task_create_failed_reboot("state_task");
  }

}


/**
 * Returns the requested state task get status information from the module
 * state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Current immutable system-status snapshot.
 */
system_status_t state_task_get_status(void){
  // Return an effective status snapshot for consumers. The stored state
  // message may be replaced in this returned copy by an active error message.
  // Callers shall not re-classify errors or select alternative SD text.
  system_status_t out = copy_status_snapshot_();

  const error_code_t active_err = error_manager_get_active();
  out.last_error = (int32_t)active_err;
  if(active_err != ERR_NONE){
    // While an SD clear/ack flow is pending, keep showing the positive
    // recoverable message instead of briefly flashing the original SD fault
    // text before READY is reached.
    if(error_manager_is_sd_error(active_err) && sd_error_show_ok_clear()){
      out.message_id = MSG_SD_OK_CLR;
    } else {
      const msg_id_t err_msg = error_manager_get_display_message();
      if(err_msg != MSG_NONE){
        out.message_id = err_msg;
      }
    }
  }

  out.wifi_active = web_task_is_enabled();
  return out;
}

/**
 * Latches a UI request to start recording.
 *
 * The State task consumes this command in ST_READY and applies the same normal
 * start gates as the hardware RECORD button: settings complete, calibration
 * valid, and no SD maintenance condition.  This function does not change state
 * directly and does not touch SD or hardware.
 *
 * Inputs: None.
 * Returns: None.
 */
void state_task_request_record_start(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_record_start_requested = true;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/**
 * Latches a UI request to stop recording.
 *
 * The State task consumes this command in ST_RECORDING and uses the same
 * STOPPING state as the hardware RECORD button, so SD close/status handling
 * remains centralized.
 *
 * Inputs: None.
 * Returns: None.
 */
void state_task_request_record_stop(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_record_stop_requested = true;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}


/** Post a one-shot manual WiFi toggle request to the State task. */
void state_task_request_wifi_toggle(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.wifi_toggle = true;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Post a one-shot AUTO RECORDING selection toggle to the State task. */
void state_task_request_auto_recording_toggle(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.auto_recording_toggle = true;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Post a one-shot AUTO WIFI selection toggle to the State task. */
void state_task_request_auto_wifi_toggle(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.auto_wifi_toggle = true;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Post a one-shot AUTO DELETE selection toggle to the State task. */
void state_task_request_auto_delete_toggle(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.auto_delete_toggle = true;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Post a one-shot language selection toggle to the State task. */
void state_task_request_language_toggle(void){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.language_toggle = true;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Post a requested calendar date; State owns cache/NVS update. */
void state_task_request_set_date(uint16_t year, uint8_t month, uint8_t day){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.set_date = true;
  s_ui_control_requests.year = year;
  s_ui_control_requests.month = month;
  s_ui_control_requests.day = day;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Post a requested clock time; State owns cache/NVS update. */
void state_task_request_set_time(uint8_t hour, uint8_t minute){
  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.set_time = true;
  s_ui_control_requests.hour = hour;
  s_ui_control_requests.minute = minute;
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}

/** Post a requested registration; State owns normalization/NVS update. */
void state_task_request_set_registration(const char *registration){
  if(registration == nullptr){
    return;
  }

  portENTER_CRITICAL(&s_ui_cmd_mux);
  s_ui_control_requests.set_registration = true;
  strncpy(s_ui_control_requests.registration, registration, SETTINGS_REGISTRATION_LEN - 1u);
  s_ui_control_requests.registration[SETTINGS_REGISTRATION_LEN - 1u] = '\0';
  portEXIT_CRITICAL(&s_ui_cmd_mux);
}
