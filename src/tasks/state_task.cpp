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
#include "src/services/task_helpers.h"

#include "src/global.h"
#include "src/services/error_manager.h"
#include "src/services/button_hold_helpers.h"
#include "src/services/device_service.h"
#include "src/services/sd_files.h"        // SD file-management authorization gate
#include "src/services/ring_buffer.h"
#include "src/services/timebase.h"
#include "src/services/record_format.h"
#include "src/services/settings_store.h" // settings cache (registration/wifi)
#include "src/services/datetime_service.h"
#include "src/services/calibration_service.h"
#include "src/services/watchdog_service.h"

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
// Persistent watchdog fault acknowledgement latch.  When set, startup
// waits for Power/Clear before normal BOOT checks continue.
static bool s_watchdog_ack_pending = false;
// Settings persistence is owned by settings_store. The State task only reads.

// UI request latches (simple, no queues).
// UI tasks may run on another core, therefore the two command flags are
// protected by a small spinlock.  The State task consumes the flags and still
// owns all recorder-state transitions.
static portMUX_TYPE s_ui_cmd_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_ui_record_start_requested = false;
static bool s_ui_record_stop_requested = false;

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
static bool handle_error_clear_request(void){
  const bool clear_requested = (test_power_button(POWER_CLEAR_HOLD_MS) == true);
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
 * Updates the published USB status snapshot and USB-loss edge latch used by
 * READY shutdown and UI power indicators.
 *
 * Inputs: `prev_usb_present`, `prev_usb_valid`.
 * Returns: None.
 */
static void update_usb_status_snapshot(bool* prev_usb_present, bool* prev_usb_valid){
  if((prev_usb_present == nullptr) || (prev_usb_valid == nullptr)){
    return;
  }

  // Without a valid PMU, USB status cannot be trusted.  Reset both the
  // published value and the edge-detection history.
  if(!pmu_ok){
    s_st.usb_present = false;
    s_st.usb_present_valid = false;
    s_st.usb_lost = false;
    *prev_usb_valid = false;
    return;
  }

  bool usb_now = false;
  const bool ok = usb_present(&usb_now);
  s_st.usb_present_valid = ok;
  if(ok){
    // usb_lost is an edge latch: it is raised only when the previous valid
    // sample was USB-present and the current valid sample is USB-absent.
    if(*prev_usb_valid && *prev_usb_present && !usb_now){
      s_st.usb_lost = true;
    }

    // USB re-appearance clears the edge latch.  READY exit cleanup also
    // consumes it when shutdown handling starts.
    if(usb_now){
      s_st.usb_lost = false;
    }

    s_st.usb_present = usb_now;
    *prev_usb_present = usb_now;
    *prev_usb_valid = true;
  } else {
    // A failed read must not create a false USB-lost edge.
    s_st.usb_lost = false;
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
  } else {
    error_manager_raise(ERR_PMU_FAULT);
  }

  s_battery_low_cached = battery_low();
}



// =============================================================================
// Recording recurring action
// =============================================================================

/**
 * Recording service samples the acceleration sensor, builds the recording
 * block, feeds the ring buffer, and latches sensor or ring-buffer errors for
 * the state machine.
 *
 * Inputs: None.
 * Returns: None.
 */
static void recording_service(void){
  // In RECORDING, acquire samples and push to ring buffer.
  // Note: SD task drains the ring buffer by polling; no notification required.
  // (This keeps communication simple and avoids extra queues.)

  accel_sample_t sample;
  int32_t ts_ms = 0;
  if(!accel_read_xyz_bounded(&sample, &ts_ms)){
    // Sampling failure is latched here; ST_RECORDING transitions to ST_ERROR
    // after recording_service() returns.
    error_manager_raise(ERR_ACCEL_NO_RESPONSE);
    return;
  }

  record_block_t blk;
  record_format_build_block(&blk, ts_ms, &sample);
  if(!ring_buffer_push(&blk)){
    // Buffer overflow is latched here; ST_RECORDING transitions to ST_ERROR
    // after recording_service() returns.
    error_manager_raise(ERR_RINGBUFFER_OVERFLOW);
    return;
  }

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
  // Consume the USB-lost edge when leaving READY.
  s_st.usb_lost = false;
  // On exit from READY, WiFi/Web shall be OFF.
  web_task_set_enabled(false);
  // SD file-management shall be disabled outside READY.
  sd_files_set_authorized(false);
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

static bool low_power_on_battery_(void){
  return s_battery_low_cached &&
         s_st.usb_present_valid &&
         (!s_st.usb_present);
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
  sd_files_set_authorized(false);
  touch_enable(false);
  state_set(ST_OFF);
  set_msg(MSG_LOW_BATT);
}

/**
 * Low-power shutdown service enforces battery protection from any state when
 * the recorder is running on battery without USB power.
 *
 * Inputs: None.
 * Returns: None.
 */
static void low_power_shutdown_service_(void){
  if(!low_power_on_battery_()){
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
      sd_request_close();
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

  // USB edge detection state for the published hardware snapshot.
  static bool s_usb_prev_pub = false;
  static bool s_usb_prev_pub_valid = false;

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

    // Tick counter since last transition; used for simple periodic actions.
    s_state_tick++;

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

          // On entry to READY, WiFi shall be OFF by default.
          web_task_set_enabled(false);
          // SD file management is only allowed in READY when WiFi/Web is enabled.
          // Default is OFF because WiFi is forced OFF on READY entry.
          sd_files_set_authorized(false);

          // READY entry resets state-task-owned stop/error latches only. It
          // shall not directly clear SD-task internal state.
          s_shutdown_after_stop_requested = false;
          s_low_battery_shutdown_requested = false;
          s_low_battery_notice_active = false;
          error_manager_clear_active();

          // READY is the normal state where the UI can edit date/time settings.
          // Refresh the application date/time cache from RTC on entry unless a
          // local settings edit is waiting to be written back to RTC.
          (void)datetime_service_sync_rtc();
          calibration_service_refresh_status();

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

        // Settings are persisted by settings_store directly (UI/Web call it).
        // Incomplete settings keep READY active but prevent recording start.
        settings_t settings = {};
        const bool settings_ready = settings_storage_ok && settings_get(&settings) && settings_is_complete(&settings);
        calibration_service_refresh_status();
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
        const bool start_record_requested =
            (record_requested && (!power_pressed)) ||
            ((!wifi_active) && ui_start_requested);

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

        // SD file-management access is only allowed in READY when WiFi/Web is enabled.
        // Keep the authorization gate synchronized with the Web task enable.
        sd_files_set_authorized(wifi_active);

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
        // In READY, USB loss edge or power-long requests shutdown. Low-power
        // shutdown is handled globally after hardware housekeeping so all states
        // use the same battery-protection behavior.
        const bool trig_usb  = (s_st.usb_lost == true);
        const bool trig_pwr  = (test_power_button(POWER_SHUTDOWN_HOLD_MS) == true);

        if(trig_usb || trig_pwr){
          ready_exit_cleanup();
          state_set(ST_OFF);
          break;
        }

        // Combined record-hold plus power-pressed gesture clears settings and
        // calibration NVS data, then shuts down. If either clear fails, report
        // a fatal generic error.
        if(clear_settings_requested){
          const bool settings_cleared = settings_clear();
          const bool calibration_cleared = calibration_service_clear();

          if(settings_cleared && calibration_cleared){
            ready_exit_cleanup();
            state_set(ST_OFF);
          } else {
            error_manager_raise(ERR_FATAL_GENERIC);
            ready_exit_cleanup();
            state_set(ST_ERROR);
          }
          break;
        }

        if(settings_ready && calibration_ready && (!sd_maintenance_needed) && start_record_requested){
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

        // recording_service() performs acquisition, formatting, and ring-buffer
        // push. It may raise non-SD recorder-core errors.
        recording_service();

        // State change actions
        // Faults are evaluated before operator stop requests so the recorded
        // error cause is not hidden by a simultaneous button action.
        // 1) Non-SD recording faults raised by recording_service() stop
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
        if(low_power_on_battery_() == true){
          s_shutdown_after_stop_requested = true;
          s_low_battery_shutdown_requested = true;
          state_set(ST_STOPPING);
          break;
        }
        break;
      }

      
      case ST_STOPPING: {

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
          sd_request_close();
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
        if(low_power_on_battery_() == true){
          // Low-battery shutdown from ERROR still shows the recharge notice.
          request_low_battery_shutdown_();
          break;
        }

        // While displaying an SD error, keep the user-visible error aligned
        // with the latest SD-task classification. SD media state can change
        // while the system waits in ERROR.
        const error_code_t active_err = error_manager_get_active();
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

        if(handle_error_clear_request()){
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

      update_usb_status_snapshot(&s_usb_prev_pub, &s_usb_prev_pub_valid);
      update_battery_snapshot();
      low_power_shutdown_service_();
    }

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
